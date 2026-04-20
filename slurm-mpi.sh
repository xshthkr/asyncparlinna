#!/bin/bash
#SBATCH --job-name=mpi_coll_alltoallv
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=128          # Use 128 physical cores per node
#SBATCH --cpus-per-task=1              # One MPI rank per allocated CPU
#SBATCH --mem=0                        # Allow access to all node memory
#SBATCH --time=01:00:00
#SBATCH --partition=gpu                # Only partition available
#SBATCH --output=/home/blparne/shashank_u22cs060/logs/%6j_%x.out.log
#SBATCH --error=/home/blparne/shashank_u22cs060/logs/%6j_%x.err.log
#SBATCH --exclusive                    # Guarantee no other jobs on these nodes

# ------------------- Environment Setup -------------------

module purge
# module load openmpi/4.1.5

# Force use of 10 GbE and disable any InfiniBand attempts
export OMPI_MCA_btl=^openib,^smcuda     # Disable InfiniBand and CUDA IPC
export OMPI_MCA_btl_tcp_if_include=10.10.10.0/24    # byte level transfer, data over 10 GbE
export OMPI_MCA_oob_tcp_if_include=172.21.0.0/20    # out-of-band, management network 1 GbE

# enp33s0f0 is the 10 GbE interface
# enp193s0f0 is the 1 GbE management interface
# export OMPI_MCA_btl_tcp_if_include=enp33s0f0
# export OMPI_MCA_oob_tcp_if_include=enp193s0f0

# Pin ranks to specific cores to avoid OS scheduling interference
export OMPI_MCA_rmaps_base_mapping_policy=core
export OMPI_MCA_hwloc_base_binding_policy=core

# Disable hyperthreading usage unless explicitly testing
export OMPI_MCA_hwloc_base_use_hwthreads_as_cpus=0

# ------------------- Run the Benchmark -------------------

echo "[DEBUG] Loaded modules: $(module list 2>&1)"
echo "[DEBUG] G++ binary: $(which g++)"
echo "[DEBUG] G++ version: $(g++ --version | head -n 1)"
echo "[DEBUG] OpenMPI mpirun: $(which mpirun)"
echo "[DEBUG] OpenMPI version: $(mpirun --version | head -n 1)"
echo "[DEBUG] NVCC binary: $(which nvcc)"
echo "[DEBUG] NVCC version: $(nvcc --version | tail -n 1)"

echo "[SLURM] Job ID: $SLURM_JOB_ID"
echo "[SLURM] Starting job on $(date)"

echo "[SLURM] Running on nodes: $SLURM_JOB_NODELIST"
echo "[SLURM] MPI ranks: $((SLURM_NTASKS_PER_NODE * SLURM_NNODES))"

echo "[SLURM] Executing benchmark..."

srun --cpu-bind=cores ./your_collective_benchmark
# mpirun --bind-to core ./your_collective_benchmark

# -------------------- Post-Run Cleanup -------------------

echo "[SLURM] Job completed on $(date) with exit code $?"
module purge
