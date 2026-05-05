Motivation

Standard ParLinNa is hierarchical but synchronous. Phase 2 (inter-node) blocks the main thread while waiting for network transfers to complete. By offloading Phase 2 to a background thread (Communication Servlet), we can:
- Overlap inter-node communication of the current batch/iteration with the intra-node packing of the next
- Hide network latency by keeping the NIC busy while the CPU performs data organization
- Improve throughput through pipelining

Architecture: Communication Servlet

The Communication Servlet is a dedicated background thread responsible for driving MPI progress for Phase 2 transfers

Key Components

- *ServletContext:* Manages the background thread and a set of double-buffered work slots
- *ServletSlot:* A container for a single communication task, including metadata (`CommDescriptor`) and pre-allocated heap buffers
- *Double Buffering:* Two slots allow the main thread to prepare one (Phase 1) while the servlet executes the other (Phase 2)
- *Core Pinning:* The servlet is pinned to a NIC-affine core (using `hwloc`) to minimize context switching and cache misses, while the main thread is pinned away from the servlet's core

State Machine Handoff

1. *IDLE:* Slot is available
2. *READY:* Main thread has filled the slot and submitted it. Servlet picks it up
3. *DONE:* Servlet has completed the transfers. Main thread can now consume results or reuse the slot

Implementation Variants

1. Inter-iteration Pipelining (`ParLinNa_servlet`)

This variant overlaps Phase 2 of iteration $K$ with Phase 1 of iteration $K+1$. It requires the user to call the function repeatedly (e.g., in a loop)

- *Phase 1 (Main Thread):* Performs intra-node Bruck exchange
- *Submission:* Submits inter-node metadata and packed buffer to the servlet
- *Overlap:* Returns immediately. The *next* call to the function will wait for the slot to become available if the previous Phase 2 hasn't finished

2. Intra-call Chunking (`ParLinNa_servlet_v2`)

This variant introduces pipelining *within a single alltoallv call* by splitting the message into $C$ chunks

- *Payload Chunking:* The total data is divided into smaller chunks
- *Pipelined Execution:*
    - For chunk $c$:
        1. Main thread runs Phase 1 (intra-node) for chunk $c$
        2. Main thread submits Phase 2 for chunk $c$ to an available servlet slot
        3. Main thread moves to chunk $c+1$ immediately
- *Result Re-assembly:* Phase 2 receives data into per-chunk buffers, which are then scattered into the final receive buffer

Performance Benefits

- *Throughput:* By splitting large transfers into chunks, the network is utilized more continuously
- *Latency Hiding:* The blocking `MPI_Waitall` (or equivalent) in Phase 2 is replaced by asynchronous background progress, allowing the application to continue work
- *Efficiency:* Uses `Transparent Huge Pages (THP)` and `madvise(MADV_HUGEPAGE)` for large buffers to reduce TLB misses

Pseudocode: ParLinNa Servlet V1 Pipeline

Main Thread (Phase 1)
```cpp
function ParLinNa_Servlet_V1(comm, sendbuf, recvbuf)
    // 1. Reclaim the Slot
    // We must wait until the Servlet has finished Phase 2 for whatever
    // previous application invocation utilized this slot
    slot_idx = get_next_slot_index()
    slot = ctx.slots[slot_idx]
    
    while slot.state.load(memory_order_acquire) != IDLE do
        // Spin or yield
    end while
    
    // 2. Execute Compute-Heavy Phase 1
    // Perform local memory copies and Logarithmic Radix-Bruck 
    RadixBruck_IntraNode_Aggregation(sendbuf, slot.send_buffer)
    
    // 3. Prepare Metadata
    slot.sizes_storage = calculate_scatter_metadata()
    slot.sizes_ngroup = Number_of_Destinations
    
    // 4. Hand off to the Servlet
    // Memory_order_release flushes cache before state update
    slot.state.store(READY, memory_order_release)
    
    // Returns immediately to caller for pipelined overlap
end function
```

Servlet Thread (Phase 2)
```cpp
function Servlet_BackgroundThread(ctx)
    current_slot_idx = 0
    
    while not Shutdown do
        slot = ctx.slots[current_slot_idx]
        
        // 1. Await Work from Main Thread
        while slot.state.load(memory_order_acquire) != READY do
            if Shutdown then return
        end while
        
        // 2. Post Asynchronous Transfers
        for dest_rank in slot.destinations do
            MPI_Irecv(..., dest_rank, Tag=2, reqs[...])
            MPI_Isend(&slot.send_buffer[...], ..., dest_rank, Tag=2, reqs[...])
        end for
        
        // 3. Wait for Network Transfer
        MPI_Waitall(num_reqs, reqs, MPI_STATUSES_IGNORE)
        
        // 4. Return Slot to Main Thread
        slot.state.store(DONE, memory_order_release)
        
        current_slot_idx = (current_slot_idx + 1) mod NUM_SLOTS
    end while
end function
```
