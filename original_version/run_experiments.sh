#!/bin/bash
# Usage: ./run_experiments.sh   -> builds each variant, runs all traces, writes results.csv
cd "$(dirname "$0")/ChampSim"
WARM=${WARM:-5000000}; SIM=${SIM:-20000000}
TRACES=$(ls ../traces/*.xz)
OUT=../results.csv; echo "variant,trace,ipc,mpki" > $OUT

build() { # name predictor cxxflags
  python3 - "$1" "$2" <<'PY'
import json,sys
c=json.load(open('champsim_config.json')); c['ooo_cpu'][0]['branch_predictor']=sys.argv[2]
c['executable_name']=sys.argv[1]; json.dump(c,open('/tmp/cfg_'+sys.argv[1]+'.json','w'))
PY
  ./config.sh /tmp/cfg_$1.json >/dev/null && make -j8 >/dev/null 2>&1
}
run() { # name
  for t in $TRACES; do
    o=$(bin/$1 --warmup-instructions $WARM --simulation-instructions $SIM $t 2>&1)
    ipc=$(echo "$o" | grep -m1 -oP 'cumulative IPC: \K[0-9.]+' | tail -1)
    br=$(echo "$o" | grep -oP 'Branch Prediction Accuracy: [0-9.]+% MPKI: \K[0-9.]+' | tail -1)
    echo "$1,$(basename $t | cut -d. -f1-2),$ipc,$br" | tee -a $OUT
  done
}
for p in bimodal gshare; do build $p $p; run $p; done
# TAGE variants: name|NT|HL|LOGSZ  (storage ~ NT * 2^LOGSZ * (tag+5) bits)
VARIANTS=(
 "t4_default|4|5,15,44,130|10"
 "t2|2|8,44|10"
 "t3|3|5,20,80|10"
 "t4_short|4|2,4,8,16|10"
 "t4_long|4|8,32,128,256|10"
 "t4_small|4|5,15,44,130|8"
 "t4_big|4|5,15,44,130|12"
 "t4_huge|4|5,15,44,130|14"
)
for v in "${VARIANTS[@]}"; do IFS='|' read n nt hl lg <<< "$v"
  rm -f .csconfig/modules/branch/tage/tage.o
  export CPPFLAGS="-DTAGE_NT=$nt -DTAGE_HL=$hl -DTAGE_LOGSZ=$lg"
  build tage_$n tage; unset CPPFLAGS; run tage_$n
done
