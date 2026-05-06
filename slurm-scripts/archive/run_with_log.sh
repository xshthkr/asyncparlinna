#!/bin/bash

# Usage: run_with_log.sh <actual_binary> [args...]
# Logs execution details and exit code

log_cmd() {
	echo ">>> [$(hostname)] [rank = ${SLURM_PROCID:-0}] [pid = $$] $@"
	"$@"
	rc=$?
	echo "<<< [$(hostname)] [rank = ${SLURM_PROCID:-0}] [pid = $$] exit code: $rc"
	return $rc
}

log_cmd "$@"
