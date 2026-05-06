#!/bin/bash

#SBATCH --job-name=parinna-servlet-v1-slotpipelined-alltoallv-benchmark-128procs-2cpupertask-cpubound
#SBATCH --nodes=2
#SBATCH --ntasks=128
#SBATCH --ntasks-per-node=64
#SBATCH --cpus-per-task=2
#SBATCH --mem-per-cpu=16G
#SBATCH --partition=gpu
# #SBATCH --tres-per-task=cpu=2
#SBATCH --time=10:00:00
#SBATCH --output=/home/blparne/shashank_u22cs060/logs/%6j_%x.out.log
#SBATCH --error=/home/blparne/shashank_u22cs060/logs/%6j_%x.err.log
#SBATCH --chdir=/home/blparne/shashank_u22cs060/alltoallv-async-overlap

# ------------------- Environment Setup -------------------

module purge 

# module load anaconda3-2024.2
# module load cuda-12.8

# source /apps/compilers/anaconda3-24.2/etc/profile.d/conda.sh
# conda activate mpi_env

# ------------------- Run the Benchmark -------------------

# echo "[DEBUG] Loaded modules: $(module list 2>&1)"
# echo "[DEBUG] G++ binary: $(which g++)"
# echo "[DEBUG] G++ version: $(g++ --version | head -n 1)"
# echo "[DEBUG] OpenMPI mpirun: $(which mpirun)"
# echo "[DEBUG] OpenMPI version: $(mpirun --version | head -n 1)"
# echo "[DEBUG] OpenMPI mpcc: $(which mpicc)"
# echo "[DEBUG] OpenMPI mpcc version: $(mpicc --version | head -n 1)"
# echo "[DEBUG] OpenMPI mpcxx: $(which mpicxx)"
# echo "[DEBUG] OpenMPI mpcxx version: $(mpicxx --version | head -n 1)"
# echo "[DEBUG] Conda environment: $CONDA_DEFAULT_ENV"
# echo "[DEBUG] CUDA device name: $(nvidia-smi --query-gpu=name --format=csv,noheader)"
# echo "[DEBUG] CUDA compute capability: $(nvidia-smi --query-gpu=compute_cap --format=csv,noheader)"
# echo "[DEBUG] Multi-instance GPU support: $(nvidia-smi --query-gpu=mig.mode.current,mig.mode.pending --format=csv,noheader)"

echo "[SLURM] Running on nodes: $SLURM_JOB_NODELIST"
echo "[SLURM] Total tasks: $SLURM_NTASKS"
echo "[SLURM] Job ID: $SLURM_JOB_ID"
# echo "[DEBUG] Assigned GPUs: $CUDA_VISIBLE_DEVICES"
echo "[SLURM] Starting job on $(date)"

# PMIX cant find MUNGE MPI authentication plugin (becuase compiled with conda-forge openmpi)
# set it to native to silence warning and fallback 
export PMIX_MCA_psec=native

# echo "[BENCHMARK] Executing OSU micro benchmark for latency of alltoallv collective"
# srun c/mpi/collective/blocking/osu_alltoallv

LOOP_COUNT=1

echo ""
echo "[SLURM] Executing sasha's ParLinNa Servlet v1 basic allltoallv implementation"

for ((i=1; i<=LOOP_COUNT; i++)); do
	# echo ""
	# echo "[BENCHMARK $i]"

	# srun --cpu-bind=cores <binary> <loopcount> <ncores-per-node> <base-list>

	echo ""
	echo "[BENCHMARK] ncores-per-node = 64"
	echo ""

	# test parlinna servlet
	# echo ""
	# echo "[TESTING 1/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=100"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 4 2 100
	# echo ""
	# echo "[TESTING 2/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=1000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 4 2 1000
	# echo ""
	# echo "[TESTING 3/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=10000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 4 2 10000
	# echo ""
	# echo "[TESTING 4/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=100000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 4 2 100000
	# echo ""
	# echo "[TESTING 5/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=1000000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 4 2 1000000
	# echo ""
	# echo "[TESTING 5/10] ParLinNa_servlet n=64 r=8 bblock=2 msg_size=100"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 8 2 100
	# echo ""
	# echo "[TESTING 6/10] ParLinNa_servlet n=64 r=8 bblock=2 msg_size=1000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 8 2 1000
	# echo ""
	# echo "[TESTING 8/10] ParLinNa_servlet n=64 r=8 bblock=2 msg_size=10000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 8 2 10000
	# echo ""
	# echo "[TESTING 9/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=10000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 8 2 100000
	# echo ""
	# echo "[TESTING 10/10] ParLinNa_servlet n=64 r=4 bblock=2 msg_size=100000"
	# echo ""
	# srun --cpu-bind=cores build/tests/servlet_test 64 8 2 1000000
	# echo ""
	# echo "[TESTING COMPLETE]"
	# echo ""

	# baseline
	# job-name=baseline-alltoallv-benchmark-128procs-2cpupertask
	# srun --cpu-bind=cores build/examples/combAlltoallv 10 64 2 4 8 16 32 64

	# parlogna 
	# job-name=parlogna-alltoallv-benchmark-128procs-2cpupertask
	# srun --cpu-bind=cores build/examples/rbruckv 10 2 4 8 16 32 64

	# parlinna
	# srun --cpu-bind=cores build/examples/LTRNA 10 64 2 2 4 8 16 32 64

	# async parlinna servlet v1 base (radix list shouldnt be a string)
	# job-name=parlinna-servlet-v1-alltoallv-benchmark-128procs-2cpupertask-schedgetaffinity-hwloc-cpubound-nicpinning
	# srun --cpu-bind=cores build/tests/servlet_test_v1_configs 10 64 2 2 4 8 16 32 64

	# async parlinna servlet v1 slot pipelined (radix list shouldnt be a string)
	# job-name=parlinna-servlet-v1-slot-alltoallv-benchmark-128procs-2cpupertask-schedgetaffinity-hwloc-cpubound-nicpinning
	srun --cpu-bind=cores build/tests/servlet_test_v1_slot_configs 10 64 2 2 4 8 16 32 64

	# binding to cores is faster
	# job-name=parlinna-servlet-v2-chunk-sched_getaffinity-hwloc-alltoallv-internodetest-128procs-2cpupertask-notcpubound-yesnicpinning
	# async parlinna servlet v2 chunk pipelined (radix list shouldnt be a string)
	# srun --cpu-bind=cores build/tests/servlet_test_v2_chunk_configs 1 64 2 2 4 8 16 32 62
	# srun --cpu-bind=cores build/tests/servlet_test_v2_chunk_configs 1 32 2 2 4 8
	# srun --cpu-bind=cores build/tests/servlet_test_v2_chunk_configs 1 2 2 2 4 8

done

echo ""
echo "[SANITY CHECK] Show slurm process ID and node"
srun bash -c 'echo "rank=$SLURM_PROCID node=$SLURM_NODEID"'

rc=$?

# -------------------- Post-Run Cleanup -------------------

echo "[SLURM] Job completed on $(date) with exit code $rc"

# conda deactivate
module purge

