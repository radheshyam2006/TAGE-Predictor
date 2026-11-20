#!/bin/bash
# Rebuild all variants with mispredict_penalty=15 (executables bin/p15_*), run in parallel -> results_p15.csv
cd "$(dirname "$0")/ChampSim"
PEN=${PEN:-15}
build() { python3 - "$1" "$2" "$PEN" <<'PY'
import json,sys
c=json.load(open('champsim_config.json')); o=c['ooo_cpu'][0]
o['branch_predictor']=sys.argv[2]; o['mispredict_penalty']=int(sys.argv[3]); c['executable_name']=sys.argv[1]
json.dump(c,open('/tmp/cfg_'+sys.argv[1]+'.json','w'))
PY
  ./config.sh /tmp/cfg_$1.json >/dev/null && make -j8 >/dev/null 2>&1; }
for p in bimodal gshare; do build p15_$p $p; done
VARIANTS=("t4_default|4|5,15,44,130|10" "t2|2|8,44|10" "t3|3|5,20,80|10" "t4_short|4|2,4,8,16|10"
 "t4_long|4|8,32,128,256|10" "t4_small|4|5,15,44,130|8" "t4_big|4|5,15,44,130|12" "t4_huge|4|5,15,44,130|14")
for v in "${VARIANTS[@]}"; do IFS='|' read n nt hl lg <<< "$v"
  rm -f .csconfig/modules/branch/tage/tage.o
  CPPFLAGS="-DTAGE_NT=$nt -DTAGE_HL=$hl -DTAGE_LOGSZ=$lg" build p15_tage_$n tage
done
one() { b=$1; t=$2
  o=$(bin/$b --warmup-instructions 5000000 --simulation-instructions 20000000 $t 2>&1)
  echo "${b#p15_},$(basename $t | cut -d. -f1-2),$(echo "$o" | grep -oP '^CPU 0 cumulative IPC: \K[0-9.]+'),$(echo "$o" | grep -oP 'MPKI: \K[0-9.]+' | head -1)"; }
export -f one
echo "variant,trace,ipc,mpki" > ../results_p15.csv
for b in $(ls bin | grep '^p15_'); do for t in ../traces/*.xz; do echo "$b $t"; done; done | xargs -P 6 -L1 bash -c 'one $0 $1' >> ../results_p15.csv
