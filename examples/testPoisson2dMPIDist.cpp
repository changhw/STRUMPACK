/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 *
 */
#include <iostream>
#include "StrumpackSparseSolverMPIDist.hpp"
#include "sparse/CSRMatrix.hpp"

typedef double scalar;
//typedef int64_t integer;
typedef int integer;

using namespace strumpack;

int main(int argc, char* argv[]) {
  int thread_level;
  //MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_level);
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &thread_level);
  int myrank;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  if (thread_level != MPI_THREAD_FUNNELED && myrank == 0)
    std::cout << "MPI implementation does not support MPI_THREAD_FUNNELED"
              << std::endl;
  // if (thread_level != MPI_THREAD_MULTIPLE && myrank == 0)
  //   std::cout << "MPI implementation does not support MPI_THREAD_MULTIPLE,"
  //     " which might be needed for pt-scotch!" << std::endl;

  {
    int n = 30;
    if (argc > 1) n = atoi(argv[1]); // get grid size
    else std::cout << "# please provide grid size" << std::endl;

    StrumpackSparseSolverMPIDist<scalar,integer> spss(MPI_COMM_WORLD);
    spss.options().set_mc64job(0);
    spss.options().set_reordering_method(ReorderingStrategy::GEOMETRIC);
    spss.options().set_from_command_line(argc, argv);

    // TODO directly build the distributed sparse matrix
    integer N = n * n;
    integer nnz = 5 * N - 4 * n;
    CSRMatrix<scalar,integer> A(N, nnz);
    integer* col_ptr = A.get_ptr();
    integer* row_ind = A.get_ind();
    scalar* val = A.get_val();

    nnz = 0;
    col_ptr[0] = 0;
    for (integer row=0; row<n; row++) {
      for (integer col=0; col<n; col++) {
        integer ind = col+n*row;
        val[nnz] = 4.0;
        row_ind[nnz++] = ind;
        if (col > 0)  { val[nnz] = -1.0; row_ind[nnz++] = ind-1; } // left
        if (col < n-1){ val[nnz] = -1.0; row_ind[nnz++] = ind+1; } // right
        if (row > 0)  { val[nnz] = -1.0; row_ind[nnz++] = ind-n; } // up
        if (row < n-1){ val[nnz] = -1.0; row_ind[nnz++] = ind+n; } // down
        col_ptr[ind+1] = nnz;
      }
    }
    A.set_symmetric_sparsity();

    /** build distributed matrix from complete replicated original matrix ***/
    /** TODO directly construct distributed matrix **************************/
    CSRMatrixMPI<scalar,integer> Adist(&A, MPI_COMM_WORLD, false);

    // TODO, delete the global A

    auto n_local = Adist.local_rows();
    std::vector<scalar> b(n_local, scalar(1.)), x(n_local, scalar(0.));
    spss.set_matrix(Adist);
    spss.reorder(n, n);
    spss.factor();
    spss.solve(b.data(), x.data());

    auto scaled_res = Adist.max_scaled_residual(x.data(), b.data());
    if (!myrank)
      std::cout << "# COMPONENTWISE SCALED RESIDUAL = "
                << scaled_res << std::endl;
  }
  TimerList::Finalize();
  scalapack::Cblacs_exit(1);
  MPI_Finalize();
  return 0;
}
