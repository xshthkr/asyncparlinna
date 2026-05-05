1. ParLogNa (Bruck-style) Complexity

ParLogNa reduces the number of steps in the all-to-all exchange by using an $r$-ary representation of process ranks

Step Count
The number of communication steps $K$ is determined by the number of digits required to represent the number of processes $P$ in radix $r$:
$$K = \lceil \log_r P \rceil \times (r - 1)$$
Wait, more accurately in the implementation (`parlogna.cpp`):
$$w = \lceil \log_r P \rceil$$
The total number of rounds is:
$$K = w(r-1) - d$$
where $d$ accounts for the "highest digit" cases where we don't need a full set of $r-1$ steps for the last power of $r$

Cost Model
The total time $T_{log}$ is:
$$T_{log} = \sum_{k=1}^K (\alpha + \beta \cdot m_k + L \cdot \gamma)$$
- $\alpha$: Latency per step
- $\beta$: Inverse bandwidth
- $m_k$: Size of data moved in step $k$
- $L$: Local memory copy cost factor

2. ParLinNa (Hierarchical) Model

ParLinNa splits the communication into two distinct phases

Phase 1: Intra-node (Logarithmic)
Total processes $P$ are split into $G$ groups (nodes) of $n$ processes each
$$T_{phase1} = \lceil \log_r n \rceil (r-1) \cdot (\alpha_{local} + \beta_{local} \cdot m_{local})$$

Phase 2: Inter-node (Linear)
Processes at rank $i$ within their respective nodes communicate with the same rank $i$ in other nodes (scatter-like)
$$T_{phase2} = (G - 1) \cdot (\alpha_{remote} + \beta_{remote} \cdot m_{remote})$$

Total Time (Synchronous)
$$T_{sync} = T_{phase1} + T_{phase2}$$

3. Asynchronous Overlap Model

The introduction of the communication servlet and chunking allows for pipelined execution

Inter-iteration Overlap (`ParLinNa_servlet`)
When calling the algorithm in a loop of $N$ iterations:
$$T_{total} \approx T_{p1,1} + \sum_{i=1}^{N-1} \max(T_{p1,i+1}, T_{p2,i}) + T_{p2,N}$$
If $T_{p1} \approx T_{p2}$, we achieve nearly $2\times$ throughput compared to the synchronous version

Intra-call Chunking (`ParLinNa_servlet_v2`)
The message is split into $C$ chunks. Let $T_{p1}(M/C)$ be the time for Phase 1 on a chunk of size $M/C$
$$T_{total} = T_{p1}(M/C) + \sum_{j=1}^{C-1} \max(T_{p1}(M/C), T_{p2}(M/C)) + T_{p2}(M/C) + T_{scatter}$$
- *Speedup Condition:* Overlap is effective when $\max(T_{p1}, T_{p2})$ is significantly smaller than $T_{p1} + T_{p2}$
- *Optimal $C$:* A larger $C$ increases the number of pipeline stages but also increases the overhead of `MPI_Isend/Irecv` (more $O(\alpha)$ hits) and the final `T_{scatter}` re-assembly

4. Buffer Capacity Mathematics
To avoid reallocations, the servlet pre-allocates buffers based on the maximum message size encountered
$$Capacity \ge \max_{i,j} (sendcounts[i][j]) \times P \times typesize$$
In the chunked version, the capacity per slot is scaled by $1/C$, but additional re-assembly buffers are required:
$$Total\_Memory = NUM\_SLOTS \times (Slot\_Buffers) + Reassembly\_Buffer$$
