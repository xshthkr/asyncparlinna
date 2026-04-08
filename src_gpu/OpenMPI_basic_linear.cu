/*
 * OpenMPI_basic_linear.cu
 * 
 *  Created on: April 08, 2026
 *      Author: xshthkr
 *
 * Adapted from kokofan's implementation of the basic linear algorithm for MPI_Alltoallv, with CUDA support for GPU buffers.
*/

#include <gpu_async_alltoallv.h>

int gpu_ompi_alltoallv_intra_basic_linear(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype, 
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype, 
    MPI_Comm comm)
{
    int i, size, rank, err, nreqs;
    int sdtype_size, rdtype_size;
    char *d_psnd, *d_prcv;
    MPI_Aint sext, rext;
    MPI_Request *preq;

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    /* Get extent of send and recv types */
    MPI_Type_size(sendtype, &sdtype_size);
    MPI_Type_size(recvtype, &rdtype_size);

    MPI_Type_get_extent(sendtype, NULL, &sext);
    MPI_Type_get_extent(recvtype, NULL, &rext);

    /* Simple optimization - handle send to self first */
    d_psnd = (char *) d_sendbuf + sdispls[rank] * sext;
    d_prcv = (char *) d_recvbuf + rdispls[rank] * rext;
    size_t bytes_to_copy = recvcounts[rank] * rext;
    // memcpy(d_prcv, d_psnd, recvcounts[rank]*rext);
    cudaError_t cuda_err = cudaMemcpy(d_prcv, d_psnd, bytes_to_copy, cudaMemcpyDeviceToDevice);
    if (cudaSuccess != cuda_err) {
        fprintf(stderr, "Rank %d: CUDA memcpy self copy failed: %s\n", rank, cudaGetErrorString(cuda_err));
        return -1;
    }

    if (sendcounts[rank] < 0) { return -1; }

    /* If only one process, we're done. */
    if (1 == size) { return MPI_SUCCESS; }

    /* Now, initiate all send/recv to/from others. */
    nreqs = 0;
    preq = (MPI_Request *)malloc(2 * (size-1) * sizeof(MPI_Request));

    /* Post all receives first */
    for (i = 0; i < size; ++i) {
        if (i == rank) { continue; }

        if (0 < recvcounts[i]) {
            d_prcv = (char *) (d_recvbuf + rdispls[i] * rext);
            err = MPI_Irecv(d_prcv, recvcounts[i], recvtype, i, 0, comm, &preq[nreqs]);
            if (MPI_SUCCESS != err) { return -1; }
           	nreqs++;
        }
    }

    /* Now post all sends */
    for (i = 0; i < size; ++i) {
        if (i == rank) { continue; }

        if (0 < sendcounts[i]) {
            d_psnd = (char *) (d_sendbuf + sdispls[i] * sext);
            err = MPI_Isend(d_psnd, sendcounts[i], sendtype, i, 0, comm, &preq[nreqs]);
            if (MPI_SUCCESS != err) { return -1; }
            nreqs++;
        }
    }

    /* Wait for them all.  If there's an error, note that we don't care
     * what the error was -- just that there *was* an error.  The PML
     * will finish all requests, even if one or more of them fail.
     * i.e., by the end of this call, all the requests are free-able.
     * So free them anyway -- even if there was an error, and return the
     * error after we free everything. */
    err = MPI_Waitall(nreqs, preq, MPI_STATUSES_IGNORE);
    /* Free the requests in all cases as they are persistent */
    free(preq);

    return MPI_SUCCESS;
}

