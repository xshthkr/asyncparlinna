/*
 * parlinna_servlet_v2.cpp
 *
 * true single-call phase 1 / phase 2 pipelining via payload chunking
 *
 * splits the message payload into C chunks, then pipelines:
 *   chunk 0: phase 1 -> submit phase 2 on slot 0
 *   chunk 1: phase 1 -> submit phase 2 on slot 1 (overlaps with chunk 0 transfer)
 *   ...
 *
 * phase 2 receives into per-chunk temp buffers (contiguous layout),
 * then a final scatter copies data to the correct positions in recvbuf
 *
 *      Author: xshthkr
 */

#include "async.h"
#include "comm_servlet.h"
#include "mpi.h"
#include "utils.h"

#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace async_rbruck_alltoallv {

static void wait_slot_available(ServletSlot *slot) {
	while (true) {
		int s { slot->state.load(std::memory_order_acquire) };
		if (s == static_cast<int>(ServletState::IDLE)) return;
		if (s == static_cast<int>(ServletState::DONE)) {
			slot->state.store(static_cast<int>(ServletState::IDLE), std::memory_order_release);
			return;
		}
	}
}

static void ensure_slot_capacity(ServletSlot *slot, size_t send_bytes, int ngroup) {
	if (send_bytes > slot->send_buffer_capacity) {
		if (slot->send_buffer) free(slot->send_buffer);
		slot->send_buffer = (char*) malloc(send_bytes);
		slot->send_buffer_capacity = send_bytes;
	}
	if (ngroup > slot->sizes_ngroup) {
		if (slot->sizes_storage) free(slot->sizes_storage);
		slot->sizes_storage = (int*) malloc(4 * ngroup * sizeof(int));
		slot->sizes_ngroup = ngroup;
	}
}

/*
run phase 1 bruck on a chunk, pack the slot descriptor for phase 2
phase 2 receives into chunk_recvbuf (a temp buffer, NOT the final recvbuf)
*/
static int run_phase1_chunk(
	int n, int r, int nprocs, int typesize,
	int ngroup, int sw, int grank, int gid,
	char *sendbuf_base, int *chunk_sendcounts, int *chunk_sdispls,
	int *chunk_recvcounts,
	char *chunk_recvbuf,  // temp buffer for this chunk's phase 2 recv
	MPI_Comm comm, int bblock, ServletSlot *slot)
{
	int imax { rbruck_alltoallv_utils::pow(r, sw-1) * ngroup };
	int max_sd { (ngroup > imax) ? ngroup : imax };

	int local_max_count { 0 };
	int max_send_count { 0 };
	int id { 0 };
	int updated_sentcounts[nprocs];
	int rotate_index_array[nprocs];
	int pos_status[nprocs];
	int sent_blocks[max_sd];

	for (int i { 0 }; i < nprocs; i++) {
		if (chunk_sendcounts[i] > local_max_count)
			local_max_count = chunk_sendcounts[i];
	}
	MPI_Allreduce(&local_max_count, &max_send_count, 1, MPI_INT, MPI_MAX, comm);

	size_t required { static_cast<size_t>(max_send_count) * typesize * nprocs };
	ensure_slot_capacity(slot, required, ngroup);
	char *temp_send_buffer { slot->send_buffer };

	for (int i { 0 }; i < ngroup; i++) {
		int gsp { i * n };
		for (int j { 0 }; j < n; j++) {
			rotate_index_array[id++] = gsp + (2 * grank - j + n) % n;
		}
	}

	memset(pos_status, 0, nprocs * sizeof(int));
	memcpy(updated_sentcounts, chunk_sendcounts, nprocs * sizeof(int));

	char *extra_buffer { (char*) malloc(max_send_count * typesize * nprocs) };
	char *temp_recv_buffer { (char*) malloc(max_send_count * typesize * max_sd) };

	// PHASE 1: intra-node bruck
	int spoint { 1 }, distance { 1 }, next_distance { r }, di { 0 };

	for (int x { 0 }; x < sw; x++) {
		for (int z { 1 }; z < r; z++) {
			di = 0;
			spoint = z * distance;
			if (spoint > n - 1) break;

			for (int g { 0 }; g < ngroup; g++) {
				for (int i { spoint }; i < n; i += next_distance) {
					for (int j { i }; j < (i + distance); j++) {
						if (j > n - 1) break;
						int id { g * n + (j + grank) % n };
						sent_blocks[di++] = id;
					}
				}
			}

			int metadata_send[di];
			int sendCount { 0 }, offset { 0 };
			for (int i { 0 }; i < di; i++) {
				int send_index { rotate_index_array[sent_blocks[i]] };
				metadata_send[i] = updated_sentcounts[send_index];
				if (pos_status[send_index] == 0)
					memcpy(&temp_send_buffer[offset],
						   &sendbuf_base[chunk_sdispls[send_index] * typesize],
						   updated_sentcounts[send_index] * typesize);
				else
					memcpy(&temp_send_buffer[offset],
						   &extra_buffer[sent_blocks[i] * max_send_count * typesize],
						   updated_sentcounts[send_index] * typesize);
				offset += updated_sentcounts[send_index] * typesize;
			}

			int recv_proc { gid * n + (grank + spoint) % n };
			int send_proc { gid * n + (grank - spoint + n) % n };

			int metadata_recv[di];
			MPI_Sendrecv(metadata_send, di, MPI_INT, send_proc, 0,
						 metadata_recv, di, MPI_INT, recv_proc, 0,
						 comm, MPI_STATUS_IGNORE);
			for (int i { 0 }; i < di; i++) sendCount += metadata_recv[i];

			MPI_Sendrecv(temp_send_buffer, offset, MPI_CHAR, send_proc, 1,
						 temp_recv_buffer, sendCount * typesize, MPI_CHAR, recv_proc, 1,
						 comm, MPI_STATUS_IGNORE);

			offset = 0;
			for (int i { 0 }; i < di; i++) {
				int send_index { rotate_index_array[sent_blocks[i]] };
				memcpy(&extra_buffer[sent_blocks[i] * max_send_count * typesize],
					   &temp_recv_buffer[offset], metadata_recv[i] * typesize);
				offset += metadata_recv[i] * typesize;
				pos_status[send_index] = 1;
				updated_sentcounts[send_index] = metadata_recv[i];
			}
		}
		distance *= r;
		next_distance *= r;
	}

	// organize data into temp_send_buffer for inter-node scatter
	int index { 0 };
	for (int i { 0 }; i < nprocs; i++) {
		int d { updated_sentcounts[rotate_index_array[i]] * typesize };
		if (grank == (i % n))
			memcpy(&temp_send_buffer[index],
				   &sendbuf_base[chunk_sdispls[i] * typesize], d);
		else
			memcpy(&temp_send_buffer[index],
				   &extra_buffer[i * max_send_count * typesize], d);
		index += d;
	}

	free(temp_recv_buffer);
	free(extra_buffer);

	// compute per-node sizes/displs into slot storage (contiguous layout, offset 0)
	int *send_sizes  { &slot->sizes_storage[0] };
	int *send_displs { &slot->sizes_storage[ngroup] };
	int *recv_sizes  { &slot->sizes_storage[2 * ngroup] };
	int *recv_displs { &slot->sizes_storage[3 * ngroup] };

	int soff { 0 }, roff { 0 };
	for (int i { 0 }; i < ngroup; i++) {
		int nsend { 0 }, nrecv { 0 };
		for (int j { 0 }; j < n; j++) {
			int id { i * n + j };
			nsend += updated_sentcounts[rotate_index_array[id]];
			nrecv += chunk_recvcounts[id];
		}
		send_sizes[i]  = nsend * typesize;
		send_displs[i] = soff;
		recv_sizes[i]  = nrecv * typesize;
		recv_displs[i] = roff;
		soff += send_sizes[i];
		roff += recv_sizes[i];
	}

	// fill descriptor — recv into temp buffer (contiguous), NOT final recvbuf
	CommDescriptor *desc { &slot->desc };
	desc->send_buf    = temp_send_buffer;
	desc->send_sizes  = send_sizes;
	desc->send_displs = send_displs;
	desc->recv_buf    = chunk_recvbuf;  // temp buffer
	desc->recv_sizes  = recv_sizes;
	desc->recv_displs = recv_displs;
	desc->ngroup      = ngroup;
	desc->n           = n;
	desc->gid         = gid;
	desc->grank       = grank;
	desc->bblock      = bblock;
	desc->comm        = comm;

	return 0;
}


int ParLinNa_servlet_v2(
	int n, int r, int bblock, int num_chunks,
	char *sendbuf, int *sendcounts, int *sdispls, MPI_Datatype sendtype,
	char *recvbuf, int *recvcounts, int *rdispls, MPI_Datatype recvtype,
	MPI_Comm comm, ServletContext *servlet_ctx)
{
	if (r < 2) return -1;
	if (num_chunks < 1) num_chunks = 1;

	int rank, nprocs;
	MPI_Comm_rank(comm, &rank);
	MPI_Comm_size(comm, &nprocs);
	if (r > n) r = n;

	int typesize;
	MPI_Type_size(sendtype, &typesize);

	int ngroup { nprocs / n };
	int sw { static_cast<int>(ceil(log(n) / float(log(r)))) };
	int grank { rank % n };
	int gid { rank / n };

	// single chunk: skip chunking overhead, write directly to recvbuf
	if (num_chunks == 1) {
		int slot_idx { servlet_ctx->producer_idx };
		ServletSlot *slot { &servlet_ctx->slots[slot_idx] };
		wait_slot_available(slot);

		// for single chunk, chunk counts == full counts, recv directly to recvbuf
		// compute contiguous recv_displs for phase 2
		int *chunk_recvcounts { new int[nprocs] };
		memcpy(chunk_recvcounts, recvcounts, nprocs * sizeof(int));

		// compute total recv bytes for temp buffer
		int total_recv { 0 };
		for (int i { 0 }; i < nprocs; i++) total_recv += recvcounts[i];
		char *temp_recv { (char*) malloc(total_recv * typesize) };

		run_phase1_chunk(n, r, nprocs, typesize, ngroup, sw, grank, gid,
						 sendbuf, sendcounts, sdispls, chunk_recvcounts,
						 temp_recv, comm, bblock, slot);
		servlet_submit(servlet_ctx);
		servlet_wait(servlet_ctx);

		// scatter from contiguous temp buffer to recvbuf using rdispls
		int roff { 0 };
		for (int g { 0 }; g < ngroup; g++) {
			for (int j { 0 }; j < n; j++) {
				int rid { g * n + j };
				memcpy(recvbuf + rdispls[rid] * typesize,
					   temp_recv + roff,
					   recvcounts[rid] * typesize);
				roff += recvcounts[rid] * typesize;
			}
		}

		free(temp_recv);
		delete[] chunk_recvcounts;
		return 0;
	}

	// allocate per-chunk temp recv buffers
	// compute total recv size per chunk
	int total_recv_full { 0 };
	for (int i { 0 }; i < nprocs; i++) total_recv_full += recvcounts[i];

	// upper bound: each chunk's recv is at most the full recv
	char **chunk_recv_bufs { new char*[num_chunks] };
	size_t *chunk_recv_sizes { new size_t[num_chunks] };

	int *chunk_sendcounts { new int[nprocs] };
	int *chunk_sdispls    { new int[nprocs] };
	int *chunk_recvcounts { new int[nprocs] };

	// per-chunk recv layout info for the final scatter
	// chunk_recv_offsets[c][i] = offset within chunk_recv_bufs[c] for rank i's data
	int **chunk_recv_offsets { new int*[num_chunks] };
	int **chunk_recv_counts_saved { new int*[num_chunks] };

	for (int c { 0 }; c < num_chunks; c++) {
		// compute this chunk's counts and displacements
		size_t chunk_total_recv { 0 };
		for (int i { 0 }; i < nprocs; i++) {
			int base_s { sendcounts[i] / num_chunks };
			int rem_s  { sendcounts[i] % num_chunks };
			chunk_sendcounts[i] = base_s + (c < rem_s ? 1 : 0);
			chunk_sdispls[i]    = sdispls[i] + c * base_s + std::min(c, rem_s);

			int base_r { recvcounts[i] / num_chunks };
			int rem_r  { recvcounts[i] % num_chunks };
			chunk_recvcounts[i] = base_r + (c < rem_r ? 1 : 0);
			chunk_total_recv += chunk_recvcounts[i];
		}

		chunk_recv_bufs[c] = (char*) malloc(chunk_total_recv * typesize);
		chunk_recv_sizes[c] = chunk_total_recv * typesize;

		// save per-rank recv counts and compute per-rank offsets within the temp buffer
		// the temp buffer is laid out contiguously by group, then by rank within group
		chunk_recv_offsets[c] = new int[nprocs];
		chunk_recv_counts_saved[c] = new int[nprocs];
		memcpy(chunk_recv_counts_saved[c], chunk_recvcounts, nprocs * sizeof(int));

		int roff { 0 };
		for (int g { 0 }; g < ngroup; g++) {
			for (int j { 0 }; j < n; j++) {
				int rid { g * n + j };
				chunk_recv_offsets[c][rid] = roff;
				roff += chunk_recvcounts[rid] * typesize;
			}
		}

		// acquire slot, run phase 1, submit phase 2
		int slot_idx { servlet_ctx->producer_idx };
		ServletSlot *slot { &servlet_ctx->slots[slot_idx] };
		wait_slot_available(slot);

		run_phase1_chunk(n, r, nprocs, typesize, ngroup, sw, grank, gid,
						 sendbuf, chunk_sendcounts, chunk_sdispls,
						 chunk_recvcounts, chunk_recv_bufs[c],
						 comm, bblock, slot);

		servlet_submit(servlet_ctx);
		// next chunk's phase 1 overlaps with this chunk's phase 2!
	}

	// wait for all in-flight chunks
	servlet_wait(servlet_ctx);

	// final scatter: copy from contiguous temp buffers to correct recvbuf positions
	for (int c { 0 }; c < num_chunks; c++) {
		for (int i { 0 }; i < nprocs; i++) {
			int base_r { recvcounts[i] / num_chunks };
			int rem_r  { recvcounts[i] % num_chunks };
			int r_off  { c * base_r + std::min(c, rem_r) };

			memcpy(recvbuf + (rdispls[i] + r_off) * typesize,
				   chunk_recv_bufs[c] + chunk_recv_offsets[c][i],
				   chunk_recv_counts_saved[c][i] * typesize);
		}

		free(chunk_recv_bufs[c]);
		delete[] chunk_recv_offsets[c];
		delete[] chunk_recv_counts_saved[c];
	}

	delete[] chunk_recv_bufs;
	delete[] chunk_recv_sizes;
	delete[] chunk_recv_offsets;
	delete[] chunk_recv_counts_saved;
	delete[] chunk_sendcounts;
	delete[] chunk_sdispls;
	delete[] chunk_recvcounts;

	return 0;
}

}
