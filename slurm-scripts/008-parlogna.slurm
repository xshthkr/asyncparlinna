#!/bin/bash

#SBATCH --job-name=parlogna-alltoallv-benchmark-8procs-2cpupertask-cpubound
#SBATCH --nodes=2
#SBATCH --ntasks=8
#SBATCH --ntasks-per-node=4
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=16G
#SBATCH --partition=gpu
#SBATCH --time=02:00:00
#SBATCH --output=/home/blparne/shashank_u22cs060/logs/%6j_%x.out.log
#SBATCH --error=/home/blparne/shashank_u22cs060/logs/%6j_%x.err.log
#SBATCH --chdir=/home/blparne/shashank_u22cs060/alltoallv-async-overlap

# ------------------- Environment Setup -------------------

module purge 

# ------------------- Run the Benchmark -------------------

echo "[SLURM] Running on nodes: $SLURM_JOB_NODELIST"
echo "[SLURM] Total tasks: $SLURM_NTASKS"
echo "[SLURM] Job ID: $SLURM_JOB_ID"
echo "[SLURM] Starting job on $(date)"

export PMIX_MCA_psec=native

echo ""
echo "[SLURM] Executing kokofan's ParLogNa allltoallv implementation"


echo ""
echo "[BENCHMARK] ncores-per-node = 4"
echo ""

# parlogna
srun --cpu-bind=cores build/examples/rbruckv 10 2 4

echo ""
echo "[SANITY CHECK] Show slurm process ID and node"
echo ""
srun bash -c 'echo "rank=$SLURM_PROCID node=$SLURM_NODEID"'

rc=$?

# -------------------- Post-Run Cleanup -------------------

echo ""
echo "[SLURM] Job completed on $(date) with exit code $rc"

module purge

