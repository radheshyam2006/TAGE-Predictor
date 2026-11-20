#!/bin/bash
# Re-run all built binaries on all traces in parallel; parse FINAL IPC/MPKI.
cd "$(dirname "$0")/ChampSim"
one() { b=$1; t=$2
  o=$(bin/$b --warmup-instructions 5000000 --simulation-instructions 20000000 $t 2>&1)
  ipc=$(echo "$o" | grep -oP '^CPU 0 cumulative IPC: \K[0-9.]+')
  mpki=$(echo "$o" | grep -oP 'MPKI: \K[0-9.]+' | head -1)
  echo "$b,$(basename $t | cut -d. -f1-2),$ipc,$mpki"; }
export -f one
echo "variant,trace,ipc,mpki" > ../results_final.csv
for b in $(ls bin | grep -v '^champsim$'); do for t in ../traces/*.xz; do echo "$b $t"; done; done \
 | xargs -P 6 -L1 bash -c 'one $0 $1' >> ../results_final.csv
