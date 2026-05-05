ParLogNa (Parameterized Logarithmic Non-uniform Alltoallv) is an algorithm designed to optimize small-message `MPI_Alltoallv` performance by minimizing communication latency. It generalizes Bruck's algorithm by introducing an adjustable radix r

Key Concepts

- Logarithmic Complexity: Reduces the number of communication rounds from O(P) to O(log_r P), where P is the number of processes
- Adjustable Radix (r): Allows tuning the balance between the number of steps (latency) and the amount of data sent per step (congestion)
- Non-uniform Support: Unlike standard Bruck's algorithm, ParLogNa handles varying message sizes by exchanging metadata (counts) before data transfers in each round

Parameters

- Radix ($r$): The base of the communication tree. A larger r reduces the number of steps but increases the number of messages sent in each step
- Block size ($b$): Controls the number of concurrent non-blocking transfers within a single step to manage network injection rate

Phases of Algorithm

1. Metadata Exchange: In each step, processes exchange the sizes of the data blocks they are about to send
2. Data Rotation & Exchange: Data is packed based on its destination in the logical $r$-ary representation and exchanged with peers
3. Local Copy/Replace: Received data is either placed in its final position in the receive buffer or stored in a temporary buffer for the next round of the logarithmic exchange

Performance Trade-offs

- Small Messages: Extremely effective as it minimizes the number of O(1) latency-bound steps
- Large Messages: May perform worse than linear algorithms due to multiple memory copies and potential network congestion from multi-step forwarding
- Heterogeneity: Does not explicitly account for node hierarchy (intra-node vs. inter-node), which is addressed by ParLinNa
