#!/bin/bash

SCRIPT_DIR="/home/blparne/shashank_u22cs060/alltoallv-async-overlap/slurm-scripts"
MAX_JOBS=2

submit_job() {
    sbatch "$1"
}

running_jobs() {
    squeue -u "$USER" -h -t RUNNING,PD | wc -l
}

for script in "$SCRIPT_DIR"/*.slurm; do
    # Wait until we have fewer than MAX_JOBS running/pending
    while [ "$(running_jobs)" -ge "$MAX_JOBS" ]; do
        sleep 60 
    done

    echo "Submitting $script"
    submit_job "$script"
done
