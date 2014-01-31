/**
 * \file matrix.hpp
 * \author Dhairya Malhotra, dhairya.malhotra88@gmail.com
 * \date 2-11-2011
 * \brief This file contains definition of the class Matrix.
 */

#ifndef _MATRIX_HPP_
#define _MATRIX_HPP_

#include <cstdlib>
#include <vector>
#include <iostream>
#include <vector.hpp>

#ifdef __INTEL_OFFLOAD
#pragma offload_attribute(push,target(mic))
#endif

template <class T>
class Permutation;

template <class T>
class Matrix{

  template <class Y>
  friend std::ostream& operator<<(std::ostream& output, const Matrix<Y>& M);

  public:

  struct
#ifdef __INTEL_OFFLOAD
  __attribute__ ((target(mic)))
#endif
  Device{

    Device& operator=(Matrix& M){
      dim[0]=M.Dim(0);
      dim[1]=M.Dim(1);
      dev_ptr=(uintptr_t)M[0];
      return *this;
    }

    inline T* operator[](size_t j) const{
      return &((T*)dev_ptr)[j*dim[1]];
    }

    size_t dim[2];
    uintptr_t dev_ptr;
    int lock_idx;
  };

  Matrix();

  Matrix(size_t dim1, size_t dim2, T* data_=NULL, bool own_data_=true);

  Matrix(const Matrix<T>& M);

  ~Matrix();

  void Swap(Matrix<T>& M);

  void ReInit(size_t dim1, size_t dim2, T* data_=NULL, bool own_data_=true);

  Device& AllocDevice(bool copy);

  void Device2Host(T* host_ptr=NULL);

  void Device2HostWait();

  void FreeDevice(bool copy);

  void Write(const char* fname);

  size_t Dim(size_t i) const;

  void Resize(size_t i, size_t j);

  void SetZero();

  Matrix<T>& operator=(const Matrix<T>& M);

  Matrix<T>& operator+=(const Matrix<T>& M);

  Matrix<T>& operator-=(const Matrix<T>& M);

  Matrix<T> operator+(const Matrix<T>& M2);

  Matrix<T> operator-(const Matrix<T>& M2);

  T& operator()(size_t i,size_t j) const;

  T* operator[](size_t i) const;

  Matrix<T> operator*(const Matrix<T>& M);

  static void DGEMM(Matrix<T>& M_r, const Matrix<T>& A, const Matrix<T>& B, T beta=0.0);

  void RowPerm(const Permutation<T>& P);
  void ColPerm(const Permutation<T>& P);

  Matrix<T> Transpose();

  static void Transpose(Matrix<T>& M_r, const Matrix<T>& M);

  Matrix<T> pinv();

  Matrix<T> pinv(T eps);

  private:

  size_t dim[2];
  T* data_ptr;
  bool own_data;

  Device dev;
  Vector<char> dev_sig;
};


/**
 * /brief P=[e(p1)*s1 e(p2)*s2 ... e(pn)*sn],
 * where e(k) is the kth unit vector,
 * perm := [p1 p2 ... pn] is the permutation vector,
 * scal := [s1 s2 ... sn] is the scaling vector.
 */
#define PERM_INT_T size_t
template <class T>
class Permutation{

  template <class Y>
  friend std::ostream& operator<<(std::ostream& output, const Permutation<Y>& P);

  public:

  Permutation(){}

  Permutation(size_t size);

  static Permutation<T> RandPerm(size_t size);

  Matrix<T> GetMatrix() const;

  size_t Dim() const;

  Permutation<T> Transpose();

  Permutation<T> operator*(const Permutation<T>& P);

  Matrix<T> operator*(const Matrix<T>& M);

  template <class Y>
  friend Matrix<Y> operator*(const Matrix<Y>& M, const Permutation<Y>& P);

  Vector<PERM_INT_T> perm;
  Vector<T> scal;
};

#include <matrix.txx>

#ifdef __INTEL_OFFLOAD
#pragma offload_attribute(pop)
#endif

#endif
