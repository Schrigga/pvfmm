/**
 * \file mem_mgr.hpp
 * \author Dhairya Malhotra, dhairya.malhotra@gmail.com
 * \date 6-30-2014
 * \brief This file contains the definition of a simple memory manager which
 * uses a pre-allocated buffer of size defined in call to the constructor.
 */

#ifndef _PVFMM_MEM_MGR_HPP_
#define _PVFMM_MEM_MGR_HPP_

#include <map>
#include <stack>
#include <vector>
#include <cassert>
#include <iostream>
#include <cmath>
#include <omp.h>
#include <pvfmm_common.hpp>
#include <mem_utils.hpp>

#ifdef __INTEL_OFFLOAD
#pragma offload_attribute(push,target(mic))
#endif
namespace pvfmm{
namespace mem{

class MemoryManager{

  public:

    MemoryManager(size_t N){
      buff_size=N;
      buff=(char*)::malloc(buff_size); assert(buff);
      n_dummy_indx=new_node();
      size_t n_indx=new_node();
      node& n_dummy=node_buff[n_dummy_indx-1];
      node& n=node_buff[n_indx-1];

      n_dummy.size=0;
      n_dummy.free=false;
      n_dummy.prev=0;
      n_dummy.next=n_indx;
      n_dummy.mem_ptr=&buff[0];
      assert(n_indx);

      n.size=N;
      n.free=true;
      n.prev=n_dummy_indx;
      n.next=0;
      n.mem_ptr=&buff[0];
      n.it=free_map.insert(std::make_pair(N,n_indx));

      omp_init_lock(&omp_lock);
    }

    ~MemoryManager(){
      node* n=&node_buff[n_dummy_indx-1];
      n=&node_buff[n->next-1];
      if(n==NULL || !n->free || n->size!=buff_size ||
          node_stack.size()!=node_buff.size()-2){
        std::cout<<"\nWarning: memory leak detected.\n";
      }

      omp_destroy_lock(&omp_lock);
      if(buff) ::free(buff);
    }

    void* malloc(size_t size){
      size_t alignment=MEM_ALIGN;
      assert(alignment <= 0x8000);
      if(!size) return NULL;
      size+=sizeof(size_t) + --alignment + 2;
      std::multimap<size_t, size_t>::iterator it;
      uintptr_t r=0;

      omp_set_lock(&omp_lock);
      it=free_map.lower_bound(size);
      if(it==free_map.end()){ // Use system malloc
        r = (uintptr_t)::malloc(size);
      }else if(it->first==size){ // Found exact size block
        size_t n_indx=it->second;
        node& n=node_buff[n_indx-1];
        //assert(n.size==it->first);
        //assert(n.it==it);
        //assert(n.free);

        n.free=false;
        free_map.erase(it);
        ((size_t*)n.mem_ptr)[0]=n_indx;
        r = (uintptr_t)&((size_t*)n.mem_ptr)[1];
      }else{ // Found larger block.
        size_t n_indx=it->second;
        size_t n_free_indx=new_node();
        node& n_free=node_buff[n_free_indx-1];
        node& n     =node_buff[n_indx-1];
        //assert(n.size==it->first);
        //assert(n.it==it);
        //assert(n.free);

        n_free=n;
        n_free.size-=size;
        n_free.mem_ptr=(char*)n_free.mem_ptr+size;
        n_free.prev=n_indx;
        if(n_free.next){
          size_t n_next_indx=n_free.next;
          node& n_next=node_buff[n_next_indx-1];
          n_next.prev=n_free_indx;
        }

        n.free=false;
        n.size=size;
        n.next=n_free_indx;

        free_map.erase(it);
        n_free.it=free_map.insert(std::make_pair(n_free.size,n_free_indx));
        ((size_t*)n.mem_ptr)[0]=n_indx;
        r = (uintptr_t) &((size_t*)n.mem_ptr)[1];
      }
      omp_unset_lock(&omp_lock);

      uintptr_t o = (uintptr_t)(r + 2 + alignment) & ~(uintptr_t)alignment;
      ((uint16_t*)o)[-1] = (uint16_t)(o-r);
      return (void*)o;
    }

    void free(void* p_){
      if(!p_) return;
      void* p=((void*)((uintptr_t)p_-((uint16_t*)p_)[-1]));
      if(p<&buff[0] || p>=&buff[buff_size]) return ::free(p);

      size_t n_indx=((size_t*)p)[-1];
      assert(n_indx>0 && n_indx<=node_buff.size());
      ((size_t*)p)[-1]=0;

      omp_set_lock(&omp_lock);
      node& n=node_buff[n_indx-1];
      assert(!n.free && n.size>0 && n.mem_ptr==&((size_t*)p)[-1]);
      n.free=true;

      if(n.prev!=0 && node_buff[n.prev-1].free){
        size_t n_prev_indx=n.prev;
        node& n_prev=node_buff[n_prev_indx-1];
        free_map.erase(n_prev.it);
        n.size+=n_prev.size;
        n.mem_ptr=n_prev.mem_ptr;
        n.prev=n_prev.prev;
        delete_node(n_prev_indx);

        if(n.prev){
          size_t n_prev_indx=n.prev;
          node& n_prev=node_buff[n_prev_indx-1];
          n_prev.next=n_indx;
        }
      }
      if(n.next!=0 && node_buff[n.next-1].free){
        size_t n_next_indx=n.next;
        node& n_next=node_buff[n_next_indx-1];
        free_map.erase(n_next.it);
        n.size+=n_next.size;
        n.next=n_next.next;
        delete_node(n_next_indx);

        if(n.next){
          size_t n_next_indx=n.next;
          node& n_next=node_buff[n_next_indx-1];
          n_next.prev=n_indx;
        }
      }
      n.it=free_map.insert(std::make_pair(n.size,n_indx));
      omp_unset_lock(&omp_lock);
    }

    void print(){
      if(!buff_size) return;
      omp_set_lock(&omp_lock);

      size_t size=0;
      size_t largest_size=0;
      node* n=&node_buff[n_dummy_indx-1];
      std::cout<<"\n|";
      while(n->next){
        n=&node_buff[n->next-1];
        if(n->free){
          std::cout<<' ';
          largest_size=std::max(largest_size,n->size);
        }
        else{
          std::cout<<'#';
          size+=n->size;
        }
      }
      std::cout<<"|  allocated="<<round(size*1000.0/buff_size)/10<<"%";
      std::cout<<"  largest_free="<<round(largest_size*1000.0/buff_size)/10<<"%\n";

      omp_unset_lock(&omp_lock);
    }

    static void test(){
      size_t M=2000000000;
      { // With memory manager
        size_t N=M*sizeof(double)*1.1;
        double tt;
        double* tmp;

        std::cout<<"With memory manager: ";
        MemoryManager memgr(N);

        for(size_t j=0;j<3;j++){
          tmp=(double*)memgr.malloc(M*sizeof(double)); assert(tmp);
          tt=omp_get_wtime();
          #pragma omp parallel for
          for(size_t i=0;i<M;i+=64) tmp[i]=i;
          tt=omp_get_wtime()-tt;
          std::cout<<tt<<' ';
          memgr.free(tmp);
        }
        std::cout<<'\n';
      }
      { // Without memory manager
        double tt;
        double* tmp;

        //pvfmm::MemoryManager memgr(N);

        std::cout<<"Without memory manager: ";
        for(size_t j=0;j<3;j++){
          tmp=(double*)::malloc(M*sizeof(double)); assert(tmp);
          tt=omp_get_wtime();
          #pragma omp parallel for
          for(size_t i=0;i<M;i+=64) tmp[i]=i;
          tt=omp_get_wtime()-tt;
          std::cout<<tt<<' ';
          ::free(tmp);
        }
        std::cout<<'\n';
      }
    }

  private:

    struct node{
      bool free;
      size_t size;
      void* mem_ptr;
      size_t prev, next;
      std::multimap<size_t, size_t>::iterator it;
    };

    MemoryManager();

    MemoryManager(const MemoryManager& m);

    size_t new_node(){
      if(node_stack.empty()){
        node_buff.resize(node_buff.size()+1);
        node_stack.push(node_buff.size());
      }

      size_t indx=node_stack.top();
      node_stack.pop();
      assert(indx);
      return indx;
    }

    void delete_node(size_t indx){
      assert(indx);
      assert(indx<=node_buff.size());
      node& n=node_buff[indx-1];
      n.size=0;
      n.prev=0;
      n.next=0;
      n.mem_ptr=NULL;
      node_stack.push(indx);
    }

    char* buff;
    size_t buff_size;
    std::vector<node> node_buff;
    std::stack<size_t> node_stack;
    std::multimap<size_t, size_t> free_map;
    size_t n_dummy_indx;

    omp_lock_t omp_lock;
};

}//end namespace
}//end namespace
#ifdef __INTEL_OFFLOAD
#pragma offload_attribute(pop)
#endif

#endif //_PVFMM_MEM_MGR_HPP_
