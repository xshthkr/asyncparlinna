/*
 * servlet_test.cpp
 *
 * tests ParLinNa_servlet correctness and pipelining performance
 *
 * 1. MPI_Alltoallv          (baseline)
 * 2. ParLinNa_coalesced     (reference)
 * 3. ParLinNa_servlet       (single-shot, with wait)
 * 4. ParLinNa_servlet       (pipelined, multi-iteration overlap)
 *
 * correctness: all results compared against MPI_Alltoallv
 *
 * usage: mpirun -n <nprocs> ./servlet_test <n> <r> <bblock> <msg_size>
 *  n        = ranks per node (group size)
 *  r        = bruck radix
 *  bblock   = batching block size
 *  msg_size = elements per destination
 *
 *      Author: xshthkr
 */

#include "../async/async.h"
#include "../async/comm_servlet.h"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <mpi.h>

static const int NUM_ITERS = 10;

int main(int argc, char **argv) {

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        std::cerr << "ERROR: MPI_THREAD_MULTIPLE not supported" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (argc < 5) {
        if (rank == 0)
            std::cout << "Usage: mpirun -n <nprocs> " << argv[0]
                      << " <n> <r> <bblock> <msg_size>" << std::endl;
        MPI_Finalize();
        return 1;
    }

    int n { atoi(argv[1]) };
    int r { atoi(argv[2]) };
    int bblock { atoi(argv[3]) };
    int msg_size { atoi(argv[4]) };

    if (nprocs % n != 0) {
        if (rank == 0)
            std::cerr << "ERROR: nprocs (" << nprocs << ") must be divisible by n (" << n << ")" << std::endl;
        MPI_Finalize();
        return 1;
    }

    // build uniform sendcounts (msg_size elements to each rank)
    int sendcounts[nprocs], recvcounts[nprocs];
    int sdispls[nprocs], rdispls[nprocs];

    for (int i { 0 }; i < nprocs; i++) { sendcounts[i] = msg_size; }

    MPI_Alltoall(sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT, MPI_COMM_WORLD);

    int soffset { 0 }, roffset { 0 };
    for (int i { 0 }; i < nprocs; i++) {
        sdispls[i] = soffset;
        rdispls[i] = roffset;
        soffset += sendcounts[i];
        roffset += recvcounts[i];
    }

    // fill send buffer: rank * 1000 + dest
    long long *sendbuf { new long long[soffset] };
    int idx { 0 };
    for (int i { 0 }; i < nprocs; i++) {
        for (int j { 0 }; j < sendcounts[i]; j++) {
            sendbuf[idx++] = rank * 1000 + i;
        }
    }

    /* 1. MPI_Alltoallv baseline */
    long long *recv_mpi { new long long[roffset] };
    memset(recv_mpi, 0, roffset * sizeof(long long));

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 { MPI_Wtime() };
    MPI_Alltoallv(sendbuf, sendcounts, sdispls, MPI_LONG_LONG,
                  recv_mpi, recvcounts, rdispls, MPI_LONG_LONG,
                  MPI_COMM_WORLD);
    double t_mpi { MPI_Wtime() - t0 };

    /* 2. ParLinNa_coalesced */
    long long *recv_coal { new long long[roffset] };
    memset(recv_coal, 0, roffset * sizeof(long long));

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();
    async_rbruck_alltoallv::ParLinNa_coalesced(
        n, r, bblock,
        (char*)sendbuf, sendcounts, sdispls, MPI_LONG_LONG,
        (char*)recv_coal, recvcounts, rdispls, MPI_LONG_LONG,
        MPI_COMM_WORLD);
    double t_coal { MPI_Wtime() - t0 };

    /* 3. ParLinNa_servlet (single-shot) */
    long long *recv_srv { new long long[roffset] };
    memset(recv_srv, 0, roffset * sizeof(long long));

    async_rbruck_alltoallv::ServletConfig cfg { async_rbruck_alltoallv::servlet_default_config() };
    async_rbruck_alltoallv::ServletContext servlet_ctx;
    async_rbruck_alltoallv::servlet_init(&servlet_ctx, &cfg);

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();
    async_rbruck_alltoallv::ParLinNa_servlet(
        n, r, bblock,
        (char*)sendbuf, sendcounts, sdispls, MPI_LONG_LONG,
        (char*)recv_srv, recvcounts, rdispls, MPI_LONG_LONG,
        MPI_COMM_WORLD, &servlet_ctx);
    async_rbruck_alltoallv::servlet_wait(&servlet_ctx);
    double t_srv { MPI_Wtime() - t0 };

    /* 4. ParLinNa_servlet pipelined (multi-iteration) */
    long long **recv_pipe { new long long*[NUM_ITERS] };
    for (int i { 0 }; i < NUM_ITERS; i++) {
        recv_pipe[i] = new long long[roffset];
        memset(recv_pipe[i], 0, roffset * sizeof(long long));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();
    for (int i { 0 }; i < NUM_ITERS; i++) {
        async_rbruck_alltoallv::ParLinNa_servlet(
            n, r, bblock,
            (char*)sendbuf, sendcounts, sdispls, MPI_LONG_LONG,
            (char*)recv_pipe[i], recvcounts, rdispls, MPI_LONG_LONG,
            MPI_COMM_WORLD, &servlet_ctx);
    }
    async_rbruck_alltoallv::servlet_wait(&servlet_ctx);
    double t_pipe { MPI_Wtime() - t0 };

    async_rbruck_alltoallv::servlet_shutdown(&servlet_ctx);

    /* correctness checks against MPI_Alltoallv */
    int errors_coal { 0 }, errors_srv { 0 }, errors_pipe { 0 };

    for (int i { 0 }; i < roffset; i++) {
        if (recv_mpi[i] != recv_coal[i]) errors_coal++;
        if (recv_mpi[i] != recv_srv[i]) errors_srv++;
    }
    for (int iter { 0 }; iter < NUM_ITERS; iter++) {
        for (int i { 0 }; i < roffset; i++) {
            if (recv_mpi[i] != recv_pipe[iter][i]) errors_pipe++;
        }
    }

    int total_coal { 0 }, total_srv { 0 }, total_pipe { 0 };
    MPI_Reduce(&errors_coal, &total_coal, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&errors_srv, &total_srv, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&errors_pipe, &total_pipe, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    double max_t_mpi { 0 }, max_t_coal { 0 }, max_t_srv { 0 }, max_t_pipe { 0 };
    MPI_Reduce(&t_mpi, &max_t_mpi, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_coal, &max_t_coal, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_srv, &max_t_srv, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_pipe, &max_t_pipe, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Sasha's beloved ParLinNa Servlet Test" << std::endl;
        std::cout << "procs=" << nprocs << " n=" << n << " r=" << r
                  << " bblock=" << bblock << " msg_size=" << msg_size << std::endl;
        std::cout << "MPI_Alltoallv:       " << max_t_mpi << "s" << std::endl;
        std::cout << "coalesced:           " << max_t_coal << "s" << std::endl;
        std::cout << "servlet (1 iter):    " << max_t_srv << "s" << std::endl;
        std::cout << "servlet pipelined:   " << max_t_pipe << "s (" << NUM_ITERS << " iters)" << std::endl;

        double per_iter_pipe { max_t_pipe / NUM_ITERS };
        std::cout << "  per-iter pipelined: " << per_iter_pipe << "s" << std::endl;
        if (per_iter_pipe < max_t_srv) {
            double speedup { (max_t_srv - per_iter_pipe) / max_t_srv * 100.0 };
            std::cout << "  overlap speedup:    " << speedup << "%" << std::endl;
        }

        bool all_pass { total_coal == 0 && total_srv == 0 && total_pipe == 0 };
        if (all_pass) {
            std::cout << "PASS: all results match MPI_Alltoallv (yipee)" << std::endl;
        } else {
            if (total_coal > 0) std::cout << "FAIL: coalesced " << total_coal << " mismatches" << std::endl;
            if (total_srv > 0) std::cout << "FAIL: servlet " << total_srv << " mismatches" << std::endl;
            if (total_pipe > 0) std::cout << "FAIL: pipelined " << total_pipe << " mismatches" << std::endl;
            std::cout << "(womp womp)" << std::endl;
        }
    }

    delete[] sendbuf;
    delete[] recv_mpi;
    delete[] recv_coal;
    delete[] recv_srv;
    for (int i { 0 }; i < NUM_ITERS; i++) { delete[] recv_pipe[i]; }
    delete[] recv_pipe;

    MPI_Finalize();
    return (total_coal > 0 || total_srv > 0 || total_pipe > 0) ? 1 : 0;
}
