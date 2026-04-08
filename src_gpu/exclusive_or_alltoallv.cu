/*
 * exclusive_or_alltoallv.cu
 * 
 *  Created on: April 08, 2026
 *      Author: xshthkr
 *
 * Adapted from kokofan's implementation of the exclusive-or algorithm for MPI_Alltoallv
*/

#include <gpu_async_alltoallv.h>

int gpu_exclusive_or_alltoallv(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype,
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype,
    MPI_Comm comm)
{
    int rank, size, step;
    MPI_Aint sext, rext;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Check power of two
    if ((size & (size - 1)) != 0) return -1;  // simpler check

    MPI_Type_get_extent(sendtype, NULL, &sext);
    MPI_Type_get_extent(recvtype, NULL, &rext);

    // self copy
    if (recvcounts[rank] > 0) {
        void *d_src = (char*)d_sendbuf + sdispls[rank] * sext;
        void *d_dst = (char*)d_recvbuf + rdispls[rank] * rext;
        cudaMemcpy(d_dst, d_src, recvcounts[rank] * rext, cudaMemcpyDeviceToDevice);
    }

    for (step = 1; step < size; ++step) {
        int partner = rank ^ step;
        void *d_psnd = (char*)d_sendbuf + sdispls[partner] * sext;
        void *d_prcv = (char*)d_recvbuf + rdispls[partner] * rext;

        MPI_Sendrecv(d_psnd, sendcounts[partner], sendtype, partner, 0,
                     d_prcv, recvcounts[partner], recvtype, partner, 0,
                     comm, MPI_STATUS_IGNORE);
    }

    return MPI_SUCCESS;
}

