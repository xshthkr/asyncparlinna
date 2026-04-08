/*
 * exclusive_or_alltoallv.cu
 * 
 *  Created on: April 08, 2026
 *      Author: xshthkr
 *
 * Adapted from kokofan's implementation of the exclusive-or algorithm for MPI_Alltoallv
*/

#include <gpu_async_alltoallv.h>

int gpu_exclusive_or_alltoallv_s1(
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
        cudaError_t cuda_err = cudaMemcpy(d_dst, d_src, recvcounts[rank] * rext, cudaMemcpyDeviceToDevice);
        if (cudaSuccess != cuda_err) {
            fprintf(stderr, "Rank %d: CUDA memcpy self copy failed: %s\n", rank, cudaGetErrorString(cuda_err));
            return -1;
        }
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


 /* This algorithm only works when P is power of 2 */
int gpu_exclusive_or_alltoallv_s2(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype, 
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype, 
    MPI_Comm comm)
{
    int rank, size, src, dst, step;
    int sdtype_size, rdtype_size;
    void *d_psnd, *d_prcv;
    MPI_Aint sext, rext;

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    /* Check if P is power of 2 */
    int w = ceil(log(size) / float(log(2)));

//    if (rank == 0) {
//		std::cout << "Math -- exclisive-or: " << size << " " << log(size) << " " <<  log(2) << " " << w << " " << myPow(2, w) << std::endl;
//	}

    if (size != myPow(2, w)) { return -1; }

    /* Get extent of send and recv types */
    MPI_Type_size(sendtype, &sdtype_size);
    MPI_Type_size(recvtype, &rdtype_size);

    MPI_Type_get_extent(sendtype, NULL, &sext);
    MPI_Type_get_extent(recvtype, NULL, &rext);

    // self copy
    d_psnd = (char*)d_sendbuf + sdispls[rank] * sext;
    d_prcv = (char*)d_recvbuf + rdispls[rank] * rext;
    size_t bytes_to_copy = recvcounts[rank] * rext;
    cudaError_t cuda_err = cudaMemcpy(d_prcv, d_prcv, bytes_to_copy, cudaMemcpyDeviceToDevice);
    if (cudaSuccess != cuda_err) {
        fprintf(stderr, "Rank %d: CUDA memcpy self copy failed: %s\n", rank, cudaGetErrorString(cuda_err));
        return -1;
    }

    for (step = 1; step < size; step++) {
    	src = dst = rank ^ step;
    	d_psnd = (char *) (d_sendbuf + sdispls[dst]*sext);
    	d_prcv = (char *) (d_recvbuf + rdispls[src]*rext);
    	MPI_Sendrecv(d_psnd, sendcounts[dst]*sext, MPI_CHAR, dst, 0,
    			d_prcv, recvcounts[src]*rext, MPI_CHAR, src, 0, comm, MPI_STATUS_IGNORE);
    }

	return MPI_SUCCESS;
}
