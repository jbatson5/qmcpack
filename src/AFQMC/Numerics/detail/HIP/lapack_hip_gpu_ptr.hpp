///////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source
// License.  See LICENSE file in top directory for details.
//
// Copyright (c) 2020 QMCPACK developers.
//
// File developed by: Fionn Malone, malone14@llnl.gov, LLNL
//
// File created by: Fionn Malone, malone14@llnl.gov, LLNL
////////////////////////////////////////////////////////////////////////////////

#ifndef AFQMC_LAPACK_GPU_HPP
#define AFQMC_LAPACK_GPU_HPP

#include<cassert>
#include "AFQMC/Utilities/type_conversion.hpp"
#include "AFQMC/Memory/custom_pointers.hpp"
#include "AFQMC/Numerics/detail/HIP/hipblas_wrapper.hpp"
#include "AFQMC/Numerics/detail/HIP/rocsolver_wrapper.hpp"
#include "AFQMC/Numerics/detail/HIP/Kernels/setIdentity.hip.h"

namespace device
{
  using qmcplusplus::afqmc::remove_complex;
  using qmc_hip::rocsolverStatus_t;
  using qmc_hip::rocsolverHandle_t;

  template<typename T, typename R, typename I>
  inline static void hevr (char JOBZ, char RANGE, char UPLO, int N,
                         device_pointer<T> A, int LDA, T VL, T VU,int IL, int IU, T ABSTOL, int &M,
                         device_pointer<T> W, device_pointer<T> Z, int LDZ, device_pointer<I> ISUPPZ,
                         device_pointer<T> WORK, int &LWORK,
                         device_pointer<R> RWORK, int &LRWORK,
                         device_pointer<I> IWORK, int &LIWORK, int& INFO)
  {
    throw std::runtime_error("Error: hevr not implemented in gpu.");
  }

  // getrf_bufferSize
  template<typename T>
  inline static void getrf_bufferSize (const int n, const int m, device_pointer<T> a, int lda, int& lwork)
  {
    rocsolver::rocsolver_getrf_bufferSize(*a.handles.rocsolver_handle_,n, m, to_address(a), lda, &lwork);
  }

  template<typename T, typename R, typename I>
  inline static void getrf (const int n, const int m, device_pointer<T> && a, int lda,
                            device_pointer<I> && piv, int &st, device_pointer<R> work)
  {
    rocsolverStatus_t status = rocsolver::rocsolver_getrf(*a.handles.rocsolver_handle_, n, m,
                     to_address(a), lda, to_address(work), to_address(piv), to_address(piv)+n);
    hipMemcpy(&st,to_address(piv)+n,sizeof(int),hipMemcpyDeviceToHost);
    if(rocblas_status_success != status) {
      std::cerr<<" hipblas_getrf status, info: " <<status <<" " <<st <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_getrf returned error code.");
    }
  }

  // getrfBatched
  template<typename T, typename I>
  inline static void getrfBatched (const int n, device_pointer<T> * a, int lda, device_pointer<I> piv, device_pointer<I> info, int batchSize)
  {
    T **A_d;
    T **A_h;
    A_h = new T*[batchSize];
    for(int i=0; i<batchSize; i++)
      A_h[i] = to_address(a[i]);
    hipMalloc((void **)&A_d,  batchSize*sizeof(*A_h));
    hipMemcpy(A_d, A_h, batchSize*sizeof(*A_h), hipMemcpyHostToDevice);
    hipblasStatus_t status = hipblas::hipblas_getrfBatched(*(a[0]).handles.hipblas_handle, n, A_d, lda,
                                                        to_address(piv), to_address(info), batchSize);
    if(HIPBLAS_STATUS_SUCCESS != status)
      throw std::runtime_error("Error: hipblas_getrf returned error code.");
    hipFree(A_d);
    delete [] A_h;
  }

  // getri_bufferSize
  template<typename T>
  inline static void getri_bufferSize (int n, device_pointer<T> a, int lda, int& lwork)
  {
    // gpu uses getrs to invert matrix, which requires n*n workspace
    lwork = n*n;
  }

  // write separate query function to avoid hack!!!
  template<typename T, typename R, typename I>
  inline static void getri(int n, device_pointer<T> a, int lda, device_pointer<I> piv, device_pointer<R> work, int n1, int& status)
  {
    if(n1 < n*n)
      throw std::runtime_error("Error: getri<GPU_MEMORY_POINTER_TYPE> required buffer space of n*n.");
    if(lda != n)
      throw std::runtime_error("Error: getri<GPU_MEMORY_POINTER_TYPE> required lda = 1.");

    int* info;
    if(hipSuccess != hipMalloc ((void**)&info,sizeof(int))) {
      std::cerr<<" Error getri: Error allocating on GPU." <<std::endl;
      throw std::runtime_error("Error: hipMalloc returned error code.");
    }

    kernels::set_identity(n,n,to_address(work),n);
    if(rocblas_status_success != rocsolver::rocsolver_getrs(*a.handles.rocsolver_handle_, rocblas_operation_none, n, n,
                                                            to_address(a), lda,
                                                            to_address(piv),
                                                            to_address(work), n, info))
      throw std::runtime_error("Error: rocsolver_getrs returned error code.");
    hipMemcpy(to_address(a),to_address(work),n*n*sizeof(T),hipMemcpyDeviceToDevice);
    hipMemcpy(&status,info,sizeof(int),hipMemcpyDeviceToHost);
    hipFree(info);

  }

  // getriBatched
  template<typename T, typename I>
  inline static void getriBatched (int n, device_pointer<T> * a, int lda, device_pointer<I> piv, device_pointer<T> * ainv, int ldc, device_pointer<I> info, int batchSize)
  {
    T **A_d, **C_d;
    T **A_h, **C_h;
    A_h = new T*[batchSize];
    C_h = new T*[batchSize];
    for(int i=0; i<batchSize; i++) {
      A_h[i] = to_address(a[i]);
      C_h[i] = to_address(ainv[i]);
    }
    hipMalloc((void **)&A_d,  batchSize*sizeof(*A_h));
    hipMalloc((void **)&C_d,  batchSize*sizeof(*C_h));
    hipMemcpy(A_d, A_h, batchSize*sizeof(*A_h), hipMemcpyHostToDevice);
    hipMemcpy(C_d, C_h, batchSize*sizeof(*C_h), hipMemcpyHostToDevice);
    hipblasStatus_t status = hipblas::hipblas_getriBatched(*(a[0]).handles.hipblas_handle, n, A_d, lda,
                                                        to_address(piv), C_d, ldc, to_address(info), batchSize);
    if(HIPBLAS_STATUS_SUCCESS != status)
      throw std::runtime_error("Error: hipblas_getri returned error code.");
    hipFree(A_d);
    hipFree(C_d);
    delete [] A_h;
    delete [] C_h;
  }

  // matinveBatched
  template<typename T1, typename T2, typename I>
  inline static void matinvBatched (int n, device_pointer<T1> * a, int lda, device_pointer<T2> * ainv, int lda_inv, device_pointer<I> info, int batchSize)
  {
    T1 **A_d, **A_h;
    T2 **C_d, **C_h;
    A_h = new T1*[batchSize];
    C_h = new T2*[batchSize];
    for(int i=0; i<batchSize; i++) {
      A_h[i] = to_address(a[i]);
      C_h[i] = to_address(ainv[i]);
    }
    hipMalloc((void **)&A_d,  batchSize*sizeof(*A_h));
    hipMalloc((void **)&C_d,  batchSize*sizeof(*C_h));
    hipMemcpy(A_d, A_h, batchSize*sizeof(*A_h), hipMemcpyHostToDevice);
    hipMemcpy(C_d, C_h, batchSize*sizeof(*C_h), hipMemcpyHostToDevice);
    hipblasStatus_t status = hipblas::hipblas_matinvBatched(*(a[0]).handles.hipblas_handle, n,
                    A_d, lda, C_d, lda_inv, to_address(info), batchSize);
    if(HIPBLAS_STATUS_SUCCESS != status)
      throw std::runtime_error("Error: hipblas_matinv returned error code.");
    hipFree(A_d);
    hipFree(C_d);
    delete [] A_h;
    delete [] C_h;
  }

  // geqrf
  template<typename T>
  inline static void geqrf_bufferSize (int m, int n, device_pointer<T> a, int lda, int& lwork)
  {
    if(rocblas_status_success != rocsolver::rocsolver_geqrf_bufferSize(*a.handles.rocsolver_handle_,
                m, n, to_address(a), lda, &lwork))
      throw std::runtime_error("Error: rocsolver_geqrf_bufferSize returned error code.");
  }

  template<typename T>
  inline static void geqrf(int M, int N, device_pointer<T> A, const int LDA, device_pointer<T> TAU, device_pointer<T> WORK, int LWORK, int& INFO)
  {
    // allocating here for now
    int* piv;
    if(hipSuccess != hipMalloc ((void**)&piv,sizeof(int))) {
      std::cerr<<" Error geqrf: Error allocating on GPU." <<std::endl;
      throw std::runtime_error("Error: hipMalloc returned error code.");
    }

    rocsolverStatus_t status = rocsolver::rocsolver_geqrf(*A.handles.rocsolver_handle_, M, N,
                   to_address(A), LDA, to_address(TAU), to_address(WORK), LWORK, piv);
    hipMemcpy(&INFO,piv,sizeof(int),hipMemcpyDeviceToHost);
    if(rocblas_status_success != status) {
      int st;
      std::cerr<<" hipblas_geqrf status, info: " <<status <<" " <<INFO <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_geqrf returned error code.");
    }
    hipFree(piv);
  }

  // gelqf
  template<typename T>
  inline static void gelqf_bufferSize (int m, int n, device_pointer<T> a, int lda, int& lwork)
  {
      lwork = 0;
  }

  template<typename T>
  inline static void gelqf(int M, int N, device_pointer<T> A, const int LDA, device_pointer<T> TAU, device_pointer<T> WORK, int LWORK, int& INFO)
  {
      throw std::runtime_error("Error: gelqf not implemented in HIP backend. \n");
  }

 // gqr
  template<typename T>
  static void gqr_bufferSize (int m, int n, int k, device_pointer<T> a, int lda, int& lwork)
  {
    if(rocblas_status_success != rocsolver::rocsolver_gqr_bufferSize(*a.handles.rocsolver_handle_,
                                            m,n,k,to_address(a),lda,&lwork))
      throw std::runtime_error("Error: rocsolver_gqr_bufferSize returned error code.");
  }

  template<typename T>
  void static gqr(int M, int N, int K, device_pointer<T> A, const int LDA, device_pointer<T> TAU, device_pointer<T> WORK, int LWORK, int& INFO)
  {
    // allocating here for now
    int* piv;
    if(hipSuccess != hipMalloc ((void**)&piv,sizeof(int))) {
      std::cerr<<" Error gqr: Error allocating on GPU." <<std::endl;
      throw std::runtime_error("Error: hipMalloc returned error code.");
    }

    rocsolverStatus_t status = rocsolver::rocsolver_gqr(*A.handles.rocsolver_handle_, M, N, K,
                   to_address(A), LDA, to_address(TAU), to_address(WORK), LWORK, piv);
    hipMemcpy(&INFO,piv,sizeof(int),hipMemcpyDeviceToHost);
    if(rocblas_status_success != status) {
      int st;
      std::cerr<<" hipblas_gqr status, info: " <<status <<" " <<INFO <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_gqr returned error code.");
    }
    hipFree(piv);
  }

  template<typename T, typename I>
  void static gqrStrided(int M, int N, int K, device_pointer<T> A, const int LDA, const int Astride, device_pointer<T> TAU, const int Tstride, device_pointer<T> WORK, int LWORK, device_pointer<I> info, int batchSize )
  {
    rocsolverStatus_t status = rocsolver::rocsolver_gqr_strided(*A.handles.rocsolver_handle_, M, N, K,
                    to_address(A), LDA, Astride, to_address(TAU), Tstride, to_address(WORK), LWORK,
                    to_address(info), batchSize);
    if(rocblas_status_success != status) {
      std::cerr<<" hipblas_gqr_strided status: " <<status <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_gqr_strided returned error code.");
    }
  }

  // glq
  template<typename T>
  static void glq_bufferSize (int m, int n, int k, device_pointer<T> a, int lda, int& lwork)
  {
      lwork = 0;
  }

  template<typename T>
  void static glq(int M, int N, int K, device_pointer<T> A, const int LDA, device_pointer<T> TAU, device_pointer<T> WORK, int LWORK, int& INFO)
  {
      throw std::runtime_error("Error: glq not implemented in HIP backend. \n");
  }

  // batched geqrf
  template<typename T, typename I>
  inline static void geqrfBatched(int M, int N, device_pointer<T> *A, const int LDA, device_pointer<T>* TAU, device_pointer<I> info, int batchSize)
  {
    T **B_h = new T*[2*batchSize];
    T **A_h(B_h);
    T **T_h(B_h+batchSize);
    for(int i=0; i<batchSize; i++)
      A_h[i] = to_address(A[i]);
    for(int i=0; i<batchSize; i++)
      T_h[i] = to_address(TAU[i]);
    T **B_d;
    std::vector<int> inf(batchSize);
    hipMalloc((void **)&B_d,  2*batchSize*sizeof(*B_h));
    hipMemcpy(B_d, B_h, 2*batchSize*sizeof(*B_h), hipMemcpyHostToDevice);
    T **A_d(B_d);
    T **T_d(B_d+batchSize);
    hipblasStatus_t status = hipblas::hipblas_geqrfBatched(*(A[0]).handles.hipblas_handle, M, N, A_d, LDA,
                                                        T_d, to_address(inf.data()), batchSize);
    if(HIPBLAS_STATUS_SUCCESS!= status)
      throw std::runtime_error("Error: hipblas_geqrfBatched returned error code.");
    hipFree(B_d);
    delete [] B_h;
  }

  template<typename T, typename I>
  inline static void geqrfStrided(int M, int N, device_pointer<T> A, const int LDA, const int Astride, device_pointer<T> TAU, const int Tstride, device_pointer<I> info, int batchSize)
  {
/*
    T **B_h = new T*[2*batchSize];
    T **A_h(B_h);
    T **T_h(B_h+batchSize);
    for(int i=0; i<batchSize; i++)
      A_h[i] = to_address(A)+i*Astride;
    for(int i=0; i<batchSize; i++)
      T_h[i] = to_address(TAU)+i*Tstride;
    T **B_d;
    hipMalloc((void **)&B_d,  2*batchSize*sizeof(*B_h));
    hipMemcpy(B_d, B_h, 2*batchSize*sizeof(*B_h), hipMemcpyHostToDevice);
    T **A_d(B_d);
    T **T_d(B_d+batchSize);
*/

    std::vector<int> inf(batchSize);
    T **A_h = new T*[batchSize];
    T **T_h = new T*[batchSize];
    for(int i=0; i<batchSize; i++)
      A_h[i] = to_address(A)+i*Astride;
    for(int i=0; i<batchSize; i++)
      T_h[i] = to_address(TAU)+i*Tstride;
    T **A_d, **T_d;
    hipMalloc((void **)&A_d,  batchSize*sizeof(*A_h));
    hipMemcpy(A_d, A_h, batchSize*sizeof(*A_h), hipMemcpyHostToDevice);
    hipMalloc((void **)&T_d,  batchSize*sizeof(*T_h));
    hipMemcpy(T_d, T_h, batchSize*sizeof(*T_h), hipMemcpyHostToDevice);
    hipblasStatus_t status = hipblas::hipblas_geqrfBatched(*A.handles.hipblas_handle, M, N, A_d, LDA,
                                                        T_d, to_address(inf.data()), batchSize);
    for(int i=0; i<batchSize; i++)
      assert(inf[i]==0);
    if(HIPBLAS_STATUS_SUCCESS != status)
      throw std::runtime_error("Error: hipblas_geqrfBatched returned error code.");
    hipFree(A_d);
    delete [] A_h;
    hipFree(T_d);
    delete [] T_h;
  }

  // gesvd_bufferSize
  template<typename T>
  inline static void gesvd_bufferSize (const int m, const int n, device_pointer<T> a, int& lwork)
  {
    rocsolver::rocsolver_gesvd_bufferSize(*a.handles.rocsolver_handle_, m, n, to_address(a), &lwork);
  }

  template<typename T, typename R>
  inline static void gesvd (char jobU, char jobVT, const int m, const int n,
                            device_pointer<T> && A, int lda, device_pointer<R> && S,
                            device_pointer<T> && U, int ldu,
                            device_pointer<T> && VT, int ldvt,
                            device_pointer<T> && W, int lw,
                            int &st)
  {
    int *devSt;
    hipMalloc((void **)&devSt,  sizeof(int));
    rocsolverStatus_t status = rocsolver::rocsolver_gesvd(*A.handles.rocsolver_handle_,
                    jobU, jobVT, m, n, to_address(A), lda, to_address(S),
                    to_address(U), ldu, to_address(VT), ldvt, to_address(W), lw,
                    devSt);
    hipMemcpy(&st,devSt,sizeof(int),hipMemcpyDeviceToHost);
    if(rocblas_status_success != status) {
      std::cerr<<" hipblas_gesvd status, info: " <<status <<" " <<st <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_gesvd returned error code.");
    }
    hipFree(devSt);
  }

  template<typename T, typename R>
  inline static void gesvd (char jobU, char jobVT, const int m, const int n,
                            device_pointer<T> && A, int lda, device_pointer<R> && S,
                            device_pointer<T> && U, int ldu,
                            device_pointer<T> && VT, int ldvt,
                            device_pointer<T> && W, int lw,
                            device_pointer<R> && RW,
                            int &st)
  {
    int *devSt;
    hipMalloc((void **)&devSt,  sizeof(int));
    rocsolverStatus_t status = rocsolver::rocsolver_gesvd(*A.handles.rocsolver_handle_,
                    jobU, jobVT, m, n, to_address(A), lda, to_address(S),
                    to_address(U), ldu, to_address(VT), ldvt, to_address(W), lw,
                    devSt);
    hipMemcpy(&st,devSt,sizeof(int),hipMemcpyDeviceToHost);
    if(rocblas_status_success != status) {
      std::cerr<<" hipblas_gesvd status, info: " <<status <<" " <<st <<std::endl; std::cerr.flush();
      throw std::runtime_error("Error: hipblas_gesvd returned error code.");
    }
    hipFree(devSt);
  }


}

#endif
