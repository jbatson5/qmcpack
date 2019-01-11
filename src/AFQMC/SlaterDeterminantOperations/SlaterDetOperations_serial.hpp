//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Miguel Morales, moralessilva2@llnl.gov, Lawrence Livermore National Laboratory
//
// File created by: Miguel Morales, moralessilva2@llnl.gov, Lawrence Livermore National Laboratory 
//////////////////////////////////////////////////////////////////////////////////////

#ifndef QMCPLUSPLUS_AFQMC_SLATERDETOPERATIONS_SERIAL_HPP
#define QMCPLUSPLUS_AFQMC_SLATERDETOPERATIONS_SERIAL_HPP

#include<fstream>

#include "AFQMC/config.h"
#include "AFQMC/Numerics/ma_operations.hpp"
#include "AFQMC/Numerics/csr_blas.hpp"
#include "AFQMC/SlaterDeterminantOperations/mixed_density_matrix.hpp"
#include "AFQMC/SlaterDeterminantOperations/apply_expM.hpp"

#include "AFQMC/SlaterDeterminantOperations/SlaterDetOperations_base.hpp"

#include "mpi3/shared_communicator.hpp"
#include "type_traits/scalar_traits.h"

namespace qmcplusplus
{

namespace afqmc
{

// Implementation that doesn't use shared memory
// This version is designed for GPU and/or threading with OpenMP
template<class AllocType> 
class SlaterDetOperations_serial : public SlaterDetOperations_base<AllocType> 
{
  public:

    using communicator = boost::mpi3::shared_communicator;
    using Base = SlaterDetOperations_base<AllocType>; 
    using Alloc = typename Base::Alloc;
    using T = typename Base::T;
    using pointer = typename Base::pointer;
    using const_pointer = typename Base::const_pointer;
    using IAlloc = typename Base::IAlloc;

    using IVector = typename Base::IVector;
    using TVector = typename Base::TVector;
    using TMatrix = typename Base::TMatrix;

    using Base::MixedDensityMatrix;
    using Base::MixedDensityMatrixForWoodbury;
    using Base::MixedDensityMatrix_noHerm;
    using Base::MixedDensityMatrixFromConfiguration;
    using Base::Overlap;
    using Base::Overlap_noHerm;
    using Base::OverlapForWoodbury;
    using Base::Propagate;
    using Base::Orthogonalize;
    using Base::IWORK;
    using Base::WORK;
    using Base::TMat_NN;
    using Base::TMat_NM;
    using Base::TMat_MN;
    using Base::TMat_MM;
    using Base::TMat_MM2;
    using Base::TMat_MM3;

    SlaterDetOperations_serial(AllocType alloc_={}):
      SlaterDetOperations_base<AllocType>(alloc_)
    {
    }

    SlaterDetOperations_serial(int NMO, int NAEA, AllocType alloc_={}):
      SlaterDetOperations_base<AllocType>(NMO,NAEA,alloc_)
    {
    }

    ~SlaterDetOperations_serial() {}

    SlaterDetOperations_serial(const SlaterDetOperations_serial& other) = delete;
    SlaterDetOperations_serial(SlaterDetOperations_serial&& other) = default;
    SlaterDetOperations_serial& operator=(const SlaterDetOperations_serial& other) = delete;
    SlaterDetOperations_serial& operator=(SlaterDetOperations_serial&& other) = default;

    // C must live in shared memory for this routine to work as expected
    template<class MatA, class MatB, class MatC>
    void MixedDensityMatrix(const MatA& hermA, const MatB& B, MatC&& C, T* res, communicator& comm, bool compact=false) {
      Base::MixedDensityMatrix(hermA,B,C,res,compact);
    }

    template<class integer, class MatA, class MatB, class MatC, class MatQ>
    void MixedDensityMatrixForWoodbury(const MatA& hermA, const MatB& B, MatC&& C, T* res,
                                    integer* ref, MatQ&& QQ0, communicator& comm, bool compact=false) {
      Base::MixedDensityMatrixForWoodbury(hermA,B,std::forward<MatC>(C),res,ref,std::forward<MatQ>(QQ0),
                                    compact);
    }

    template<class MatA, class MatB>
    void Overlap(const MatA& hermA, const MatB& B, T* res, communicator& comm) { 
      Base::Overlap(hermA,B,res);
    }

    template<typename integer, class MatA, class MatB, class MatC>
    void OverlapForWoodbury(const MatA& hermA, const MatB& B, T* res, integer* ref, MatC&& QQ0, communicator& comm) {
      Base::OverlapForWoodbury(hermA,B,res,ref,std::forward<MatC>(QQ0));
    }

    template<class Mat, class MatP1, class MatV>
    void Propagate(Mat&& A, const MatP1& P1, const MatV& V, communicator& comm, int order=6) {
      Base::Propagate(std::forward<Mat>(A),P1,V,order);
    }

};

}

}

#endif
