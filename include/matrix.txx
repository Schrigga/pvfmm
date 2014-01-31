/**
 * \file matrix.txx
 * \author Dhairya Malhotra, dhairya.malhotra88@gmail.com
 * \date 2-11-2011
 * \brief This file contains inplementation of the class Matrix.
 */

#include <cstring>
#include <cassert>
#include <iomanip>
#include <typeinfo>
#include <profile.hpp>
#include <mat_utils.hpp>

template <class T>
std::ostream& operator<<(std::ostream& output, const Matrix<T>& M){
  output<<std::fixed<<std::setprecision(4)<<std::setiosflags(std::ios::left);
  for(size_t i=0;i<M.Dim(0);i++){
    for(size_t j=0;j<M.Dim(1);j++)
      output<<std::setw(10)<<M(i,j)<<' ';
    output<<";\n";
  }
  return output;
}

template <class T>
Matrix<T>::Matrix(){
  dim[0]=0;
  dim[1]=0;
  own_data=true;
  data_ptr=NULL;
  dev.dev_ptr=(uintptr_t)NULL;
}

template <class T>
Matrix<T>::Matrix(size_t dim1, size_t dim2, T* data_, bool own_data_){
  dim[0]=dim1;
  dim[1]=dim2;
  own_data=own_data_;
  if(own_data){
    if(dim[0]*dim[1]>0){
      data_ptr=mem::aligned_malloc<T>(dim[0]*dim[1]);
#ifndef __MIC__
      Profile::Add_MEM(dim[0]*dim[1]*sizeof(T));
#endif
      if(data_!=NULL) mem::memcopy(data_ptr,data_,dim[0]*dim[1]*sizeof(T));
    }else data_ptr=NULL;
  }else
    data_ptr=data_;
  dev.dev_ptr=(uintptr_t)NULL;
}

template <class T>
Matrix<T>::Matrix(const Matrix<T>& M){
  dim[0]=M.dim[0];
  dim[1]=M.dim[1];
  own_data=true;
  if(dim[0]*dim[1]>0){
    data_ptr=mem::aligned_malloc<T>(dim[0]*dim[1]);
#ifndef __MIC__
    Profile::Add_MEM(dim[0]*dim[1]*sizeof(T));
#endif
    mem::memcopy(data_ptr,M.data_ptr,dim[0]*dim[1]*sizeof(T));
  }else
    data_ptr=NULL;
  dev.dev_ptr=(uintptr_t)NULL;
}

template <class T>
Matrix<T>::~Matrix(){
  FreeDevice(false);
  if(own_data){
    if(data_ptr!=NULL){
      mem::aligned_free(data_ptr);
#ifndef __MIC__
      Profile::Add_MEM(-dim[0]*dim[1]*sizeof(T));
#endif
    }
  }
  data_ptr=NULL;
  dim[0]=0;
  dim[1]=0;
}

template <class T>
void Matrix<T>::Swap(Matrix<T>& M){
  size_t dim_[2]={dim[0],dim[1]};
  T* data_ptr_=data_ptr;
  bool own_data_=own_data;
  Device dev_=dev;
  Vector<char> dev_sig_=dev_sig;

  dim[0]=M.dim[0];
  dim[1]=M.dim[1];
  data_ptr=M.data_ptr;
  own_data=M.own_data;
  dev=M.dev;
  dev_sig=M.dev_sig;

  M.dim[0]=dim_[0];
  M.dim[1]=dim_[1];
  M.data_ptr=data_ptr_;
  M.own_data=own_data_;
  M.dev=dev_;
  M.dev_sig=dev_sig_;
}

template <class T>
void Matrix<T>::ReInit(size_t dim1, size_t dim2, T* data_, bool own_data_){
  Matrix<T> tmp(dim1,dim2,data_,own_data_);
  this->Swap(tmp);
}

template <class T>
typename Matrix<T>::Device& Matrix<T>::AllocDevice(bool copy){
  size_t len=dim[0]*dim[1];
  if(dev.dev_ptr==(uintptr_t)NULL && len>0) // Allocate data on device.
    dev.dev_ptr=DeviceWrapper::alloc_device((char*)data_ptr, len*sizeof(T));
  if(dev.dev_ptr!=(uintptr_t)NULL && copy) // Copy data to device
    dev.lock_idx=DeviceWrapper::host2device((char*)data_ptr,(char*)data_ptr,dev.dev_ptr,len*sizeof(T));

  dev.dim[0]=dim[0];
  dev.dim[1]=dim[1];
  return dev;
}

template <class T>
void Matrix<T>::Device2Host(T* host_ptr){
  dev.lock_idx=DeviceWrapper::device2host((char*)data_ptr,dev.dev_ptr,(char*)(host_ptr==NULL?data_ptr:host_ptr),dim[0]*dim[1]*sizeof(T));
}

template <class T>
void Matrix<T>::Device2HostWait(){
  DeviceWrapper::wait(dev.lock_idx);
  dev.lock_idx=-1;
}

template <class T>
void Matrix<T>::FreeDevice(bool copy){
  if(dev.dev_ptr==(uintptr_t)NULL) return;
  if(copy) DeviceWrapper::device2host((char*)data_ptr,dev.dev_ptr,(char*)data_ptr,dim[0]*dim[1]*sizeof(T));
  DeviceWrapper::free_device((char*)data_ptr, dev.dev_ptr);
  dev.dev_ptr=(uintptr_t)NULL;
  dev.dim[0]=0;
  dev.dim[1]=0;
}

template <class T>
void Matrix<T>::Write(const char* fname){
  FILE* f1=fopen(fname,"wb+");
  if(f1==NULL){
    std::cout<<"Unable to open file for writing:"<<fname<<'\n';
    return;
  }
  size_t dim_[2]={dim[0],dim[1]};
  fwrite(dim_,sizeof(size_t),2,f1);
  fwrite(data_ptr,sizeof(T),dim[0]*dim[1],f1);
  fclose(f1);
}

template <class T>
size_t Matrix<T>::Dim(size_t i) const{
  return dim[i];
}

template <class T>
void Matrix<T>::Resize(size_t i, size_t j){
  if(dim[0]==i && dim[1]==j) return;
  FreeDevice(false);
  if(own_data){
    if(data_ptr!=NULL){
      mem::aligned_free(data_ptr);
#ifndef __MIC__
      Profile::Add_MEM(-dim[0]*dim[1]*sizeof(T));
#endif

    }
  }
  dim[0]=i;
  dim[1]=j;
  if(own_data){
    if(dim[0]*dim[1]>0){
      data_ptr=mem::aligned_malloc<T>(dim[0]*dim[1]);
#ifndef __MIC__
      Profile::Add_MEM(dim[0]*dim[1]*sizeof(T));
#endif
    }else
      data_ptr=NULL;
  }
}

template <class T>
void Matrix<T>::SetZero(){
  if(dim[0]*dim[1]>0)
    memset(data_ptr,0,dim[0]*dim[1]*sizeof(T));
}

template <class T>
Matrix<T>& Matrix<T>::operator=(const Matrix<T>& M){
  if(this!=&M){
    FreeDevice(false);
    if(own_data && dim[0]*dim[1]!=M.dim[0]*M.dim[1]){
      if(data_ptr!=NULL){
        mem::aligned_free(data_ptr); data_ptr=NULL;
#ifndef __MIC__
        Profile::Add_MEM(-dim[0]*dim[1]*sizeof(T));
#endif
      }
      if(M.dim[0]*M.dim[1]>0){
        data_ptr=mem::aligned_malloc<T>(M.dim[0]*M.dim[1]);
#ifndef __MIC__
        Profile::Add_MEM(M.dim[0]*M.dim[1]*sizeof(T));
#endif
      }
    }
    dim[0]=M.dim[0];
    dim[1]=M.dim[1];
    mem::memcopy(data_ptr,M.data_ptr,dim[0]*dim[1]*sizeof(T));
  }
  return *this;
}

template <class T>
Matrix<T>& Matrix<T>::operator+=(const Matrix<T>& M){
  assert(M.Dim(0)==Dim(0) && M.Dim(1)==Dim(1));
  Profile::Add_FLOP(dim[0]*dim[1]);

  for(size_t i=0;i<M.Dim(0)*M.Dim(1);i++)
    data_ptr[i]+=M.data_ptr[i];
  return *this;
}

template <class T>
Matrix<T>& Matrix<T>::operator-=(const Matrix<T>& M){
  assert(M.Dim(0)==Dim(0) && M.Dim(1)==Dim(1));
  Profile::Add_FLOP(dim[0]*dim[1]);

  for(size_t i=0;i<M.Dim(0)*M.Dim(1);i++)
    data_ptr[i]-=M.data_ptr[i];
  return *this;
}

template <class T>
Matrix<T> Matrix<T>::operator+(const Matrix<T>& M2){
  Matrix<T>& M1=*this;
  assert(M2.Dim(0)==M1.Dim(0) && M2.Dim(1)==M1.Dim(1));
  Profile::Add_FLOP(dim[0]*dim[1]);

  Matrix<T> M_r(M1.Dim(0),M1.Dim(1),NULL);
  for(size_t i=0;i<M1.Dim(0)*M1.Dim(1);i++)
    M_r[0][i]=M1[0][i]+M2[0][i];
  return M_r;
}

template <class T>
Matrix<T> Matrix<T>::operator-(const Matrix<T>& M2){
  Matrix<T>& M1=*this;
  assert(M2.Dim(0)==M1.Dim(0) && M2.Dim(1)==M1.Dim(1));
  Profile::Add_FLOP(dim[0]*dim[1]);

  Matrix<T> M_r(M1.Dim(0),M1.Dim(1),NULL);
  for(size_t i=0;i<M1.Dim(0)*M1.Dim(1);i++)
    M_r[0][i]=M1[0][i]-M2[0][i];
  return M_r;
}

template <class T>
inline T& Matrix<T>::operator()(size_t i,size_t j) const{
  assert(i<dim[0] && j<dim[1]);
  return data_ptr[i*dim[1]+j];
}

template <class T>
inline T* Matrix<T>::operator[](size_t i) const{
  assert(i<dim[0]);
  return &data_ptr[i*dim[1]];
}

template <class T>
Matrix<T> Matrix<T>::operator*(const Matrix<T>& M){
  assert(dim[1]==M.dim[0]);
  Profile::Add_FLOP(2*(((long long)dim[0])*dim[1])*M.dim[1]);

  Matrix<T> M_r(dim[0],M.dim[1],NULL);
  mat::gemm('N','N',M.dim[1],dim[0],dim[1],
      1.0,M.data_ptr,M.dim[1],data_ptr,dim[1],0.0,M_r.data_ptr,M_r.dim[1]);
  return M_r;
}

template <class T>
void Matrix<T>::DGEMM(Matrix<T>& M_r, const Matrix<T>& A, const Matrix<T>& B, T beta){
  assert(A.dim[1]==B.dim[0]);
  assert(M_r.dim[0]==A.dim[0]);
  assert(M_r.dim[1]==B.dim[1]);
#ifndef __MIC__
  Profile::Add_FLOP(2*(((long long)A.dim[0])*A.dim[1])*B.dim[1]);
#endif
  mat::gemm('N','N',B.dim[1],A.dim[0],A.dim[1],
      1.0,B.data_ptr,B.dim[1],A.data_ptr,A.dim[1],beta,M_r.data_ptr,M_r.dim[1]);
}

#define myswap(t,a,b) {t c=a;a=b;b=c;}
template <class T>
void Matrix<T>::RowPerm(const Permutation<T>& P){
  Matrix<T>& M=*this;
  if(P.Dim()==0) return;
  assert(M.Dim(0)==P.Dim());
  size_t d0=M.Dim(0);
  size_t d1=M.Dim(1);

  #pragma omp parallel for
  for(size_t i=0;i<d0;i++){
    T* M_=M[i];
    const T s=P.scal[i];
    for(size_t j=0;j<d1;j++) M_[j]*=s;
  }

  Permutation<T> P_=P;
  for(size_t i=0;i<d0;i++)
  while(P_.perm[i]!=i){
    size_t a=P_.perm[i];
    size_t b=i;
    T* M_a=M[a];
    T* M_b=M[b];
    myswap(size_t,P_.perm[a],P_.perm[b]);
    for(size_t j=0;j<d1;j++)
      myswap(T,M_a[j],M_b[j]);
  }
}

template <class T>
void Matrix<T>::ColPerm(const Permutation<T>& P){
  Matrix<T>& M=*this;
  if(P.Dim()==0) return;
  assert(M.Dim(1)==P.Dim());
  size_t d0=M.Dim(0);
  size_t d1=M.Dim(1);

  int omp_p=omp_get_max_threads();
  Matrix<T> M_buff(omp_p,d1);

  const size_t* perm_=&(P.perm[0]);
  const T* scal_=&(P.scal[0]);
  #pragma omp parallel for
  for(size_t i=0;i<d0;i++){
    int pid=omp_get_thread_num();
    T* buff=&M_buff[pid][0];
    T* M_=M[i];
    for(size_t j=0;j<d1;j++)
      buff[j]=M_[j];
    for(size_t j=0;j<d1;j++){
      M_[j]=buff[perm_[j]]*scal_[j];
    }
  }
}
#undef myswap

#define B1 128
#define B2 32
template <class T>
Matrix<T> Matrix<T>::Transpose(){
  Matrix<T>& M=*this;
  size_t d0=M.dim[0];
  size_t d1=M.dim[1];
  Matrix<T> M_r(d1,d0,NULL);

  const size_t blk0=((d0+B1-1)/B1);
  const size_t blk1=((d1+B1-1)/B1);
  const size_t blks=blk0*blk1;
//  #pragma omp parallel for
  for(size_t k=0;k<blks;k++){
    size_t i=(k%blk0)*B1;
    size_t j=(k/blk0)*B1;
//  for(size_t i=0;i<d0;i+=B1)
//  for(size_t j=0;j<d1;j+=B1){
    size_t d0_=i+B1; if(d0_>=d0) d0_=d0;
    size_t d1_=j+B1; if(d1_>=d1) d1_=d1;
    for(size_t ii=i;ii<d0_;ii+=B2)
    for(size_t jj=j;jj<d1_;jj+=B2){
      size_t d0__=ii+B2; if(d0__>=d0) d0__=d0;
      size_t d1__=jj+B2; if(d1__>=d1) d1__=d1;
      for(size_t iii=ii;iii<d0__;iii++)
      for(size_t jjj=jj;jjj<d1__;jjj++){
        M_r[jjj][iii]=M[iii][jjj];
      }
    }
  }
//  for(size_t i=0;i<d0;i++)
//    for(size_t j=0;j<d1;j++)
//      M_r[j][i]=M[i][j];
  return M_r;
}

template <class T>
void Matrix<T>::Transpose(Matrix<T>& M_r, const Matrix<T>& M){
  size_t d0=M.dim[0];
  size_t d1=M.dim[1];
  M_r.Resize(d1, d0);

  const size_t blk0=((d0+B1-1)/B1);
  const size_t blk1=((d1+B1-1)/B1);
  const size_t blks=blk0*blk1;
  #pragma omp parallel for
  for(size_t k=0;k<blks;k++){
    size_t i=(k%blk0)*B1;
    size_t j=(k/blk0)*B1;
//  for(size_t i=0;i<d0;i+=B1)
//  for(size_t j=0;j<d1;j+=B1){
    size_t d0_=i+B1; if(d0_>=d0) d0_=d0;
    size_t d1_=j+B1; if(d1_>=d1) d1_=d1;
    for(size_t ii=i;ii<d0_;ii+=B2)
    for(size_t jj=j;jj<d1_;jj+=B2){
      size_t d0__=ii+B2; if(d0__>=d0) d0__=d0;
      size_t d1__=jj+B2; if(d1__>=d1) d1__=d1;
      for(size_t iii=ii;iii<d0__;iii++)
      for(size_t jjj=jj;jjj<d1__;jjj++){
        M_r[jjj][iii]=M[iii][jjj];
      }
    }
  }
}
#undef B2
#undef B1

template <class T>
Matrix<T> Matrix<T>::pinv(){
  T eps=(typeid(T)==typeid(float)?4*1e-5:4*1e-9);
  return pinv(eps);
}

template <class T>
Matrix<T> Matrix<T>::pinv(T eps){
  Matrix<T> M_r(dim[1],dim[0]);
  mat::pinv(data_ptr,dim[0],dim[1],eps,M_r.data_ptr);
  return M_r;
}




template <class T>
std::ostream& operator<<(std::ostream& output, const Permutation<T>& P){
  output<<std::setprecision(4)<<std::setiosflags(std::ios::left);
  size_t size=P.perm.size();
  for(size_t i=0;i<size;i++) output<<std::setw(10)<<P.perm[i]<<' ';
  output<<";\n";
  for(size_t i=0;i<size;i++) output<<std::setw(10)<<P.scal[i]<<' ';
  output<<";\n";
  return output;
}

template <class T>
Permutation<T>::Permutation(size_t size){
  perm.Resize(size);
  scal.Resize(size);
  for(size_t i=0;i<size;i++){
    perm[i]=i;
    scal[i]=1.0;
  }
}

template <class T>
Permutation<T> Permutation<T>::RandPerm(size_t size){
  Permutation<T> P(size);
  for(size_t i=0;i<size;i++){
    P.perm[i]=rand()%size;
    for(size_t j=0;j<i;j++)
      if(P.perm[i]==P.perm[j]){ i--; break; }
    P.scal[i]=((T)rand())/RAND_MAX;
  }
  return P;
}

template <class T>
Matrix<T> Permutation<T>::GetMatrix() const{
  size_t size=perm.Dim();
  Matrix<T> M_r(size,size,NULL);
  for(size_t i=0;i<size;i++)
    for(size_t j=0;j<size;j++)
      M_r[i][j]=(perm[j]==i?scal[j]:0.0);
  return M_r;
}

template <class T>
size_t Permutation<T>::Dim() const{
  return perm.Dim();
}

template <class T>
Permutation<T> Permutation<T>::Transpose(){
  size_t size=perm.Dim();
  Permutation<T> P_r(size);

  Vector<PERM_INT_T>& perm_r=P_r.perm;
  Vector<T>& scal_r=P_r.scal;
  for(size_t i=0;i<size;i++){
    perm_r[perm[i]]=i;
    scal_r[perm[i]]=scal[i];
  }
  return P_r;
}

template <class T>
Permutation<T> Permutation<T>::operator*(const Permutation<T>& P){
  size_t size=perm.Dim();
  assert(P.Dim()==size);

  Permutation<T> P_r(size);
  Vector<PERM_INT_T>& perm_r=P_r.perm;
  Vector<T>& scal_r=P_r.scal;
  for(size_t i=0;i<size;i++){
    perm_r[i]=perm[P.perm[i]];
    scal_r[i]=scal[P.perm[i]]*P.scal[i];
  }
  return P_r;
}

template <class T>
Matrix<T> Permutation<T>::operator*(const Matrix<T>& M){
  if(Dim()==0) return M;
  assert(M.Dim(0)==Dim());
  size_t d0=M.Dim(0);
  size_t d1=M.Dim(1);

  Matrix<T> M_r(d0,d1,NULL);
  for(size_t i=0;i<d0;i++){
    const T s=scal[i];
    const T* M_=M[i];
    T* M_r_=M_r[perm[i]];
    for(size_t j=0;j<d1;j++)
      M_r_[j]=M_[j]*s;
  }
  return M_r;
}

template <class T>
Matrix<T> operator*(const Matrix<T>& M, const Permutation<T>& P){
  if(P.Dim()==0) return M;
  assert(M.Dim(1)==P.Dim());
  size_t d0=M.Dim(0);
  size_t d1=M.Dim(1);

  Matrix<T> M_r(d0,d1,NULL);
  for(size_t i=0;i<d0;i++){
    const PERM_INT_T* perm_=&(P.perm[0]);
    const T* scal_=&(P.scal[0]);
    const T* M_=M[i];
    T* M_r_=M_r[i];
    for(size_t j=0;j<d1;j++)
      M_r_[j]=M_[perm_[j]]*scal_[j];
  }
  return M_r;
}

