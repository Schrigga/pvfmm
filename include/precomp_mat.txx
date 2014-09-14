/**
 * \file precomp_mat.txx
 * \author Dhairya Malhotra, dhairya.malhotra@gmail.com
 * \date 3-07-2011
 * \brief This file contains the implementation of the PrecompMat class.
 * Handles storage of precomputed translation matrices.
 */

#include <omp.h>
#include <cassert>
#include <stdint.h>
#ifdef PVFMM_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <mem_utils.hpp>
#include <profile.hpp>
#include <vector.hpp>

namespace pvfmm{

#define PRECOMP_MIN_DEPTH 40
template <class T>
PrecompMat<T>::PrecompMat(bool homogen, int max_d): homogeneous(homogen), max_depth(max_d){

  if(!homogen) mat.resize((max_d+PRECOMP_MIN_DEPTH)*Type_Count); //For each U,V,W,X
  else mat.resize(Type_Count);
  for(size_t i=0;i<mat.size();i++)
    mat[i].resize(500);

  perm.resize(Type_Count);
  for(size_t i=0;i<Type_Count;i++){
    perm[i].resize(Perm_Count);
  }

  perm_r.resize((max_d+PRECOMP_MIN_DEPTH)*Type_Count);
  perm_c.resize((max_d+PRECOMP_MIN_DEPTH)*Type_Count);
  for(size_t i=0;i<perm_r.size();i++){
    perm_r[i].resize(500);
    perm_c[i].resize(500);
  }
}

template <class T>
Matrix<T>& PrecompMat<T>::Mat(int l, Mat_Type type, size_t indx){
  int level=(homogeneous?0:l+PRECOMP_MIN_DEPTH);
  assert(level*Type_Count+type<mat.size());
  #pragma omp critical (PrecompMAT)
  if(indx>=mat[level*Type_Count+type].size()){
    mat[level*Type_Count+type].resize(indx+1);
    assert(false); //TODO: this is not thread safe.
  }
  return mat[level*Type_Count+type][indx];
}

template <class T>
Permutation<T>& PrecompMat<T>::Perm_R(int l, Mat_Type type, size_t indx){
  int level=l+PRECOMP_MIN_DEPTH;
  assert(level*Type_Count+type<perm_r.size());
  #pragma omp critical (PrecompMAT)
  if(indx>=perm_r[level*Type_Count+type].size()){
    perm_r[level*Type_Count+type].resize(indx+1);
    assert(false); //TODO: this is not thread safe.
  }
  return perm_r[level*Type_Count+type][indx];
}

template <class T>
Permutation<T>& PrecompMat<T>::Perm_C(int l, Mat_Type type, size_t indx){
  int level=l+PRECOMP_MIN_DEPTH;
  assert(level*Type_Count+type<perm_c.size());
  #pragma omp critical (PrecompMAT)
  if(indx>=perm_c[level*Type_Count+type].size()){
    perm_c[level*Type_Count+type].resize(indx+1);
    assert(false); //TODO: this is not thread safe.
  }
  return perm_c[level*Type_Count+type][indx];
}

template <class T>
Permutation<T>& PrecompMat<T>::Perm(Mat_Type type, size_t indx){
  assert(indx<Perm_Count);
  return perm[type][indx];
}

template <class T>
size_t PrecompMat<T>::CompactData(int l, Mat_Type type, Matrix<char>& comp_data, size_t offset){
  std::vector<Matrix<T> >& mat_=mat[(homogeneous?0:l+PRECOMP_MIN_DEPTH)*Type_Count+type];
  size_t mat_cnt=mat_.size();
  size_t indx_size=0;
  size_t mem_size=0;
  int omp_p=omp_get_max_threads();

  { // Determine memory size.
    indx_size+=3*sizeof(size_t); //total_size, mat_cnt, max_depth
    indx_size+=mat_cnt*(1+(2+2)*max_depth)*sizeof(size_t); //Mat, Perm_R, Perm_C.
    indx_size=((uintptr_t)indx_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);

    for(size_t j=0;j<mat_cnt;j++){
      Matrix     <T>& M =Mat   (l,type,j);
      if(M.Dim(0)>0 && M.Dim(1)>0){
        mem_size+=M.Dim(0)*M.Dim(1)*sizeof(T); mem_size=((uintptr_t)mem_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
      }

      for(size_t l=0;l<max_depth;l++){
        Permutation<T>& Pr=Perm_R(l,type,j);
        Permutation<T>& Pc=Perm_C(l,type,j);
        if(Pr.Dim()>0){
          mem_size+=Pr.Dim()*sizeof(PERM_INT_T); mem_size=((uintptr_t)mem_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
          mem_size+=Pr.Dim()*sizeof(T);          mem_size=((uintptr_t)mem_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
        }
        if(Pc.Dim()>0){
          mem_size+=Pc.Dim()*sizeof(PERM_INT_T); mem_size=((uintptr_t)mem_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
          mem_size+=Pc.Dim()*sizeof(T);          mem_size=((uintptr_t)mem_size+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
        }
      }
    }
  }
  if(comp_data.Dim(0)*comp_data.Dim(1)<offset+indx_size+mem_size){ // Resize if needed.
    Matrix<char> old_data;
    if(offset>0) old_data=comp_data;
    comp_data.Resize(1,offset+indx_size+mem_size);
    if(offset>0) mem::memcopy(comp_data[0], old_data[0], offset); //TODO: This will affect NUMA.
  }

  { // Create indx.
    char* indx_ptr=comp_data[0]+offset;
    size_t data_offset=offset+indx_size;

    ((size_t*)indx_ptr)[0]=indx_size+mem_size; indx_ptr+=sizeof(size_t);
    ((size_t*)indx_ptr)[0]= mat_cnt          ; indx_ptr+=sizeof(size_t);
    ((size_t*)indx_ptr)[0]= max_depth        ; indx_ptr+=sizeof(size_t);
    for(size_t j=0;j<mat_cnt;j++){
      Matrix     <T>& M =Mat   (l,type,j);
      ((size_t*)indx_ptr)[0]=data_offset; indx_ptr+=sizeof(size_t);
      data_offset+=M.Dim(0)*M.Dim(1)*sizeof(T); data_offset=((uintptr_t)data_offset+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);

      for(size_t l=0;l<max_depth;l++){
        Permutation<T>& Pr=Perm_R(l,type,j);
        ((size_t*)indx_ptr)[0]=data_offset; indx_ptr+=sizeof(size_t);
        data_offset+=Pr.Dim()*sizeof(PERM_INT_T); data_offset=((uintptr_t)data_offset+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
        ((size_t*)indx_ptr)[0]=data_offset; indx_ptr+=sizeof(size_t);
        data_offset+=Pr.Dim()*sizeof(T);          data_offset=((uintptr_t)data_offset+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);

        Permutation<T>& Pc=Perm_C(l,type,j);
        ((size_t*)indx_ptr)[0]=data_offset; indx_ptr+=sizeof(size_t);
        data_offset+=Pc.Dim()*sizeof(PERM_INT_T); data_offset=((uintptr_t)data_offset+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
        ((size_t*)indx_ptr)[0]=data_offset; indx_ptr+=sizeof(size_t);
        data_offset+=Pc.Dim()*sizeof(T);          data_offset=((uintptr_t)data_offset+(uintptr_t)(MEM_ALIGN-1)) & ~ (uintptr_t)(MEM_ALIGN-1);
      }
    }

  }
  { // Copy data.
    char* indx_ptr=comp_data[0]+offset;
    size_t& total_size=((size_t*)indx_ptr)[0]; indx_ptr+=sizeof(size_t);
    size_t&   mat_cnt =((size_t*)indx_ptr)[0]; indx_ptr+=sizeof(size_t);
    size_t&  max_depth=((size_t*)indx_ptr)[0]; indx_ptr+=sizeof(size_t);
    Matrix<size_t> data_offset(mat_cnt,1+(2+2)*max_depth, (size_t*)indx_ptr, false);
    offset+=total_size;

    for(size_t j=0;j<mat_cnt;j++){
      Matrix     <T>& M =Mat   (l,type,j);
      if(M.Dim(0)>0 && M.Dim(1)>0){
        #pragma omp parallel for
        for(int tid=0;tid<omp_p;tid++){
          size_t a=(M.Dim(0)*M.Dim(1)* tid   )/omp_p;
          size_t b=(M.Dim(0)*M.Dim(1)*(tid+1))/omp_p;
          mem::memcopy(comp_data[0]+data_offset[j][0]+a*sizeof(T), &M[0][a], (b-a)*sizeof(T));
        }
      }

      for(size_t l=0;l<max_depth;l++){
        Permutation<T>& Pr=Perm_R(l,type,j);
        Permutation<T>& Pc=Perm_C(l,type,j);
        if(Pr.Dim()>0){
          mem::memcopy(comp_data[0]+data_offset[j][1+4*l+0], &Pr.perm[0], Pr.Dim()*sizeof(PERM_INT_T));
          mem::memcopy(comp_data[0]+data_offset[j][1+4*l+1], &Pr.scal[0], Pr.Dim()*sizeof(         T));
        }
        if(Pc.Dim()>0){
          mem::memcopy(comp_data[0]+data_offset[j][1+4*l+2], &Pc.perm[0], Pc.Dim()*sizeof(PERM_INT_T));
          mem::memcopy(comp_data[0]+data_offset[j][1+4*l+3], &Pc.scal[0], Pc.Dim()*sizeof(         T));
        }
      }
    }
  }

  return offset;
}

template <class T>
void PrecompMat<T>::Save2File(const char* fname, bool replace){
  FILE* f=fopen(fname,"r");
  if(f!=NULL) { //File already exists.
    fclose(f);
    if(!replace) return;
  }

  f=fopen(fname,"wb");
  if(f==NULL) return;

  int tmp;
  tmp=sizeof(T);
  fwrite(&tmp,sizeof(int),1,f);
  tmp=(homogeneous?1:0);
  fwrite(&tmp,sizeof(int),1,f);
  tmp=max_depth;
  fwrite(&tmp,sizeof(int),1,f);

  for(size_t i=0;i<mat.size();i++){
    int n=mat[i].size();
    fwrite(&n,sizeof(int),1,f);

    for(int j=0;j<n;j++){
      Matrix<T>& M=mat[i][j];
      int n1=M.Dim(0);
      fwrite(&n1,sizeof(int),1,f);
      int n2=M.Dim(1);
      fwrite(&n2,sizeof(int),1,f);
      if(n1*n2>0)
      fwrite(&M[0][0],sizeof(T),n1*n2,f);
    }
  }
  fclose(f);
}

#define MY_FREAD(a,b,c,d) { \
  size_t r_cnt=fread(a,b,c,d); \
  if(r_cnt!=c){ \
    fputs ("Reading error ",stderr); \
    exit (-1); \
  } }

template <class T>
void PrecompMat<T>::LoadFile(const char* fname, MPI_Comm comm){
  Profile::Tic("LoadMatrices",&comm,true,3);

  Profile::Tic("ReadFile",&comm,true,4);
  size_t f_size=0;
  char* f_data=NULL;
  int np, myrank;
  MPI_Comm_size(comm,&np);
  MPI_Comm_rank(comm,&myrank);
  if(myrank==0){
    FILE* f=fopen(fname,"rb");
    if(f==NULL){
      f_size=0;
    }else{
      struct stat fileStat;
      if(stat(fname,&fileStat) < 0) f_size=0;
      else f_size=fileStat.st_size;

      //fseek (f, 0, SEEK_END);
      //f_size=ftell (f);
    }
    if(f_size>0){
      f_data=mem::aligned_malloc<char>(f_size);
      fseek (f, 0, SEEK_SET);
      MY_FREAD(f_data,sizeof(char),f_size,f);
      fclose(f);
    }
  }
  Profile::Toc();

  Profile::Tic("Broadcast",&comm,true,4);
  MPI_Bcast( &f_size, sizeof(size_t), MPI_BYTE, 0, comm );
  if(f_size==0){
    Profile::Toc();
    Profile::Toc();
    return;
  }

  if(f_data==NULL) f_data=mem::aligned_malloc<char>(f_size);
  char* f_ptr=f_data;
  int max_send_size=1000000000;
  while(f_size>0){
    if(f_size>(size_t)max_send_size){
      MPI_Bcast( f_ptr, max_send_size, MPI_BYTE, 0, comm );
      f_size-=max_send_size;
      f_ptr+=max_send_size;
    }else{
      MPI_Bcast( f_ptr, f_size, MPI_BYTE, 0, comm );
      f_size=0;
    }
  }

  f_ptr=f_data;
  {
    int tmp;
    tmp=*(int*)f_ptr; f_ptr+=sizeof(int);
    assert(tmp==sizeof(T));
    tmp=*(int*)f_ptr; f_ptr+=sizeof(int);
    homogeneous=tmp;
    int max_depth_;
    max_depth_=*(int*)f_ptr; f_ptr+=sizeof(int);
    size_t mat_size=(size_t)Type_Count*(homogeneous?1:max_depth_+PRECOMP_MIN_DEPTH);
    if(mat.size()<mat_size){
      max_depth=max_depth_;
      mat.resize(mat_size);
    }
    for(size_t i=0;i<mat_size;i++){
      int n;
      n=*(int*)f_ptr; f_ptr+=sizeof(int);
      if(mat[i].size()<(size_t)n)
        mat[i].resize(n);

      for(int j=0;j<n;j++){
        Matrix<T>& M=mat[i][j];
        int n1;
        n1=*(int*)f_ptr; f_ptr+=sizeof(int);
        int n2;
        n2=*(int*)f_ptr; f_ptr+=sizeof(int);
        if(n1*n2>0){
          M.Resize(n1,n2);
          mem::memcopy(&M[0][0], f_ptr, sizeof(T)*n1*n2); f_ptr+=sizeof(T)*n1*n2;
        }
      }
    }

    // Resize perm_r, perm_c
    perm_r.resize((max_depth_+PRECOMP_MIN_DEPTH)*Type_Count);
    perm_c.resize((max_depth_+PRECOMP_MIN_DEPTH)*Type_Count);
    for(size_t i=0;i<perm_r.size();i++){
      perm_r[i].resize(500);
      perm_c[i].resize(500);
    }
  }
  if(f_data!=NULL) mem::aligned_free<char>(f_data);
  Profile::Toc();
  Profile::Toc();
}

#undef MY_FREAD
#undef PRECOMP_MIN_DEPTH

template <class T>
std::vector<T>& PrecompMat<T>::RelativeTrgCoord(){
  return rel_trg_coord;
}

template <class T>
bool PrecompMat<T>::Homogen(){
  return homogeneous;
}

}//end namespace
