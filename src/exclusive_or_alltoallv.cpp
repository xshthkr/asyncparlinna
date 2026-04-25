/*
 * exclusive_or.cpp
 *
 *  Created on: Feb 20, 2024
 *      Author: kokofan
 */


#include "rbruckv.h"

 /* This algorithm only works when P is power of 2 */
int exclusive_or_alltoallv(char *sendbuf, int *sendcounts,
					       int *sdispls, MPI_Datatype sendtype, char *recvbuf,
						   int *recvcounts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm)
{
    int rank, size, src, dst, step;
    int sdtype_size, rdtype_size;
    void *psnd, *prcv;

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

    for (step = 0; step < size; step++) {
    	src = dst = rank ^ step;
    	psnd = (char *) (sendbuf + sdispls[dst] * sdtype_size);
    	prcv = (char *) (recvbuf + rdispls[src] * rdtype_size);
    	MPI_Sendrecv(psnd, sendcounts[dst], sendtype, dst, 0,
    			prcv, recvcounts[src], recvtype, src, 0, comm, MPI_STATUS_IGNORE);
    }

	return MPI_SUCCESS;
}


