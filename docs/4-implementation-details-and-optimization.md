Memory Management

Transparent Huge Pages (THP)
- Large communication buffers are allocated using `posix_memalign` with 2MB alignment
- `madvise(ptr, size, MADV_HUGEPAGE)` is used to explicitly request the kernel to use huge pages for these regions
- This reduces TLB (Translation Lookaside Buffer) misses, which can be a significant bottleneck when moving large amounts of data

Persistent Buffers
- The `ServletSlot` owns its heap-allocated buffers
- Buffers are only reallocated if the required capacity increases across subsequent alltoallv calls
- In steady-state execution (e.g., iterative solvers with fixed message sizes), this eliminates the overhead of frequent `malloc` and `free` calls

Threading and Affinity

Dedicated Communication Core
- The Communication Servlet is pinned to a specific hardware core using `pthread_setaffinity_np`
- *NIC Affinity:* The system attempts to automatically detect a core that is topologically close to the Network Interface Card (NIC) using `hwloc`. This minimizes latency between the CPU and the NIC
- *Isolation:* The main thread is pinned *away* from the servlet's core to prevent resource contention

Progress Thread Logic
- The servlet thread uses an *adaptive backoff* strategy. When no work is available, it sleeps for a short duration (`usleep`) that increases up to a configurable maximum (`backoff_max_us`)
- This prevents the idle servlet thread from consuming 100% CPU while still remaining responsive to new tasks

MPI Progress and Deadlock Avoidance

MPI_Testsome Loop
- Instead of using blocking `MPI_Waitall`, the servlet thread drives communication using a loop around `MPI_Testsome`
- This allows the thread to maintain high responsiveness and perform other tasks (like checking for shutdown signals) while waiting for transfers to complete

Deadlock Timeout
- A `deadlock_timeout_s` parameter is used to detect potential hangs in the `MPI_Testsome` loop
- If no progress is made within the timeout period, the servlet falls back to a safe `MPI_Waitall` to ensure completion, albeit at the cost of potential blocking

Thread Safety
- State transitions between the main thread and the servlet thread are managed using `std::atomic<int>` with `memory_order_acquire` and `memory_order_release` semantics
- This ensures a lock-free handoff protocol while guaranteeing that all data written by the main thread is visible to the servlet thread before it begins execution
