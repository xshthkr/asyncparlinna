/*
 * gpu_async_alltoallv.h
 * 
 *  Created on: April 08, 2026
 *      Author: xshthkr
 *
 * Adapted from kokofan's headers
*/

#ifndef SRC_GPU_ASYNC_ALLTOALLV_H_
#define SRC_GPU_ASYNC_ALLTOALLV_H_

#include "../rbrucks.h"
#include <cuda_runtime.h>

extern double init_time, findMax_time, rotateIndex_time, alcCopy_time,
getBlock_time, prepData_time, excgMeta_time, excgData_time, replace_time,
orgData_time, prepSP_time, SP_time;

extern double intra_time;
extern double* iteration_time;

int myPow(int x, unsigned int p);

int gpu_ompi_alltoallv_intra_basic_linear(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype, 
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype, 
    MPI_Comm comm);

int gpu_ompi_alltoallv_intra_pairwise(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype, 
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype, 
    MPI_Comm comm);

int gpu_MPICH_intra_scattered(
	int bblock, 
	char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype, 
	char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype, 
	MPI_Comm comm);

int gpu_exclusive_or_alltoallv_s1(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype,
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype,
    MPI_Comm comm);

int gpu_exclusive_or_alltoallv_s2(
    char *d_sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype,
    char *d_recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype,
    MPI_Comm comm);

#endif /* SRC_GPU_ASYNC_ALLTOALLV_H_ */