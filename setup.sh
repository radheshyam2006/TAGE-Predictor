#!/usr/bin/env bash
# Install the three predictors into a ChampSim checkout and build one binary per predictor.
# Usage:  [PEN=15] ./setup.sh ~/ChampSim      (PEN = mispredict_penalty; the reported results use 15)
set -euo pipefail
CS="${1:?path to ChampSim checkout}"; PEN="${PEN:-15}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$CS/branch"
cp -r "$HERE"/branch/tage "$HERE"/branch/bimodalp "$HERE"/branch/gsharep "$CS"/branch/
cd "$CS"
for P in tage bimodalp gsharep; do
  python3 - "$P" "$PEN" <<'PY'
import json, sys
p, pen = sys.argv[1], int(sys.argv[2])
c = json.load(open('champsim_config.json'))
c['executable_name'] = 'champsim_' + p
c['ooo_cpu'][0]['branch_predictor'] = p
c['ooo_cpu'][0]['mispredict_penalty'] = pen
json.dump(c, open('/tmp/cfg_%s.json' % p, 'w'), indent=1)
PY
  rm -f absolute.options
  ./config.sh /tmp/cfg_$P.json
  make -j"$(nproc)"
done
echo "Binaries ready (mispredict_penalty=$PEN): bin/champsim_tage bin/champsim_bimodalp bin/champsim_gsharep"
