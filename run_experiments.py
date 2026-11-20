#!/usr/bin/env python3
"""Run the experiments: bimodal / gshare / TAGE on a set of traces, collecting IPC and branch MPKI.

  python3 run_experiments.py --champsim ~/ChampSim/bin --traces ~/traces \
        --warmup 5000000 --sim 20000000 --exp E1,E2,E3 --jobs 6 --out results/results.csv

Experiments:  E0 = check against the original version's 8 variants | E1 = equal-storage comparison, 4-64 KB |
              E2 = number of tagged tables | E3 = longest / shortest history length.
Binaries expected in --champsim: champsim_bimodalp, champsim_gsharep, champsim_tage (built by setup.sh).
One process per run; results are appended to the CSV, so an interrupted sweep resumes where it stopped.
Storage counts only the predictor tables (counters + tags + u bits); history registers are excluded for all predictors.
"""
import argparse, csv, glob, itertools, json, math, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

TAG_DEFAULT, CTR, UB = 9, 3, 2

def log2floor(x): return int(math.floor(math.log2(x)))

def size_bimodal(bits):   return {'BP_LOGSIZE': log2floor(bits / 2)}, (1 << log2floor(bits / 2)) * 2
def size_gshare(bits):    ls = log2floor(bits / 2); return {'GS_LOGSIZE': ls}, (1 << ls) * 2

def geo(nt, mn=5, mx=130):
    """Geometric history lengths L(i)=round(mn*(mx/mn)^((i-1)/(nt-1))); (5,130,4) -> 5,15,44,130 as in the teammate's default."""
    if nt == 1: return [mn]
    return [int(round(mn * (mx / mn) ** (i / (nt - 1)))) for i in range(nt)]

def lens(l): return ','.join(str(x) for x in l)

def size_tage(bits, nt, tag=TAG_DEFAULT, base_frac=1 / 8):
    """Base T0 gets ~1/8 of the budget (2-bit counters); the rest is split equally over the nt tagged tables,
    each entry = tag + 3-bit ctr + 2-bit u, entries rounded DOWN to a power of two."""
    lb = max(4, log2floor(bits * base_frac / 2))
    if nt == 0:
        lb = log2floor(bits / 2); return {'TAGE_NT': 0, 'TAGE_LOGBASE': lb}, (1 << lb) * 2
    ebits = tag + CTR + UB
    lt = max(4, log2floor((bits - (1 << lb) * 2) / (nt * ebits)))
    used = (1 << lb) * 2 + nt * (1 << lt) * ebits
    return {'TAGE_NT': nt, 'TAGE_LOGBASE': lb, 'TAGE_LOGT': lt, 'TAGE_TAG': tag, 'TAGE_LENS': lens(geo(nt))}, used

def tbits(e):  # storage of an explicit TAGE config
    return (1 << e['TAGE_LOGBASE']) * 2 + e['TAGE_NT'] * (1 << e['TAGE_LOGT']) * (e['TAGE_TAG'] + CTR + UB)

# the teammate's original configuration (4 tables 5,15,44,130; 1024 entries; 9-bit tags; 8192-entry base)
ORIG = {'TAGE_NT': 4, 'TAGE_LOGBASE': 13, 'TAGE_LOGT': 10, 'TAGE_TAG': 9, 'TAGE_LENS': '5,15,44,130'}

def build_jobs(exps, budgets_kb, main_kb):
    """The experiments that were run. main_kb (default 16) is the budget used for E2 and E3."""
    jobs = []  # (exp, predictor, budget_kb, label, env, bits)
    B = lambda kb: kb * 1024 * 8
    if 'E0' in exps:        # check: the teammate's original 8 TAGE variants through this build (must match original_version/results_p15.csv)
        V = {'t4_default': {}, 't2': {'TAGE_NT': 2, 'TAGE_LENS': '8,44'}, 't3': {'TAGE_NT': 3, 'TAGE_LENS': '5,20,80'},
             't4_short': {'TAGE_LENS': '2,4,8,16'}, 't4_long': {'TAGE_LENS': '8,32,128,256'},
             't4_small': {'TAGE_LOGT': 8}, 't4_big': {'TAGE_LOGT': 12}, 't4_huge': {'TAGE_LOGT': 14}}
        e, b = size_bimodal(4 * 1024 * 8); jobs.append(('E0_replicate', 'bimodal', 4, 'bimodal', e, b))
        e, b = size_gshare(4 * 1024 * 8);  jobs.append(('E0_replicate', 'gshare', 4, 'gshare', e, b))
        for name, ov in V.items():
            e = {**ORIG, **ov}; jobs.append(('E0_replicate', 'tage', 0, name, e, tbits(e)))
    if 'E1' in exps:        # bimodal, gshare, TAGE at equal storage
        for kb in budgets_kb:
            e, b = size_bimodal(B(kb)); jobs.append(('E1', 'bimodal', kb, 'bimodal', e, b))
            e, b = size_gshare(B(kb));  jobs.append(('E1', 'gshare', kb, 'gshare', e, b))
            e, b = size_tage(B(kb), 4); jobs.append(('E1', 'tage', kb, 'tage_nt4', e, b))
    if 'E2' in exps:        # number of tagged tables, same total storage
        for nt in [1, 2, 3, 4, 6]:
            e, b = size_tage(B(main_kb), nt); jobs.append(('E2', 'tage', main_kb, f'nt={nt}', e, b))
    if 'E3' in exps:        # history lengths: longest (shortest = 5) and shortest (longest = 130), 4 tables
        base, b = size_tage(B(main_kb), 4)
        for mx in [20, 60, 130, 300, 600]:
            jobs.append(('E3a_maxh', 'tage', main_kb, f'maxh={mx}', {**base, 'TAGE_LENS': lens(geo(4, 5, mx))}, b))
        for mn in [2, 10, 20]:
            jobs.append(('E3b_minh', 'tage', main_kb, f'minh={mn}', {**base, 'TAGE_LENS': lens(geo(4, mn, 130))}, b))
    return jobs

def run_one(binpath, trace, env_extra, warm, sim):
    env = dict(os.environ, **{k: str(v) for k, v in env_extra.items()}, TAGE_STATS_FILE=os.devnull)
    cmd = [binpath, '--warmup-instructions', str(warm), '--simulation-instructions', str(sim), '--hide-heartbeat', trace]
    out = subprocess.run(cmd, env=env, capture_output=True, text=True).stdout
    ipc = re.findall(r'CPU 0 cumulative IPC: ([0-9.]+) instructions: (\d+) cycles: (\d+)', out)
    mpki = re.findall(r'MPKI: ([0-9.]+)', out)
    if not ipc or not mpki: return None
    return float(ipc[-1][0]), int(ipc[-1][1]), int(ipc[-1][2]), float(mpki[0])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--champsim', default='~/ChampSim/bin'); ap.add_argument('--traces', required=True)
    ap.add_argument('--warmup', type=int, default=20_000_000); ap.add_argument('--sim', type=int, default=100_000_000)
    ap.add_argument('--out', default='results/results.csv'); ap.add_argument('--exp', default='E1,E2,E3')
    ap.add_argument('--budgets', default='4,8,16,32,64'); ap.add_argument('--main-kb', type=int, default=16)
    ap.add_argument('--jobs', type=int, default=os.cpu_count()); ap.add_argument('--tag', default='', help='label stored in CSV (e.g. synthetic)')
    a = ap.parse_args()
    binmap = {'bimodal': 'champsim_bimodalp', 'gshare': 'champsim_gsharep', 'tage': 'champsim_tage'}
    bindir = os.path.expanduser(a.champsim)
    traces = sorted(glob.glob(os.path.join(os.path.expanduser(a.traces), '*.champsimtrace*')) + glob.glob(os.path.join(os.path.expanduser(a.traces), '*.trace*')))
    if not traces: sys.exit('no traces found in ' + a.traces)
    jobs = build_jobs(a.exp.split(','), [int(x) for x in a.budgets.split(',')], a.main_kb)
    done = set()
    if os.path.exists(a.out):
        for r in csv.DictReader(open(a.out)): done.add((r['exp'], r['predictor'], r['label'], r['budget_kb'], r['trace']))
    os.makedirs(os.path.dirname(a.out) or '.', exist_ok=True)
    new = not os.path.exists(a.out)
    f = open(a.out, 'a', newline=''); w = csv.writer(f)
    if new: w.writerow(['exp', 'predictor', 'label', 'budget_kb', 'trace', 'storage_bits', 'ipc', 'mpki', 'instructions', 'cycles', 'params', 'warmup', 'sim', 'tag'])
    todo = [(j, t) for j in jobs for t in traces if (j[0], j[1], j[3], str(j[2]), os.path.basename(t)) not in done]
    print(f'{len(todo)} runs ({len(jobs)} configs x {len(traces)} traces)', flush=True)
    def work(item):
        (exp, pred, kb, label, env, bits), t = item
        return item, run_one(os.path.join(bindir, binmap[pred]), t, env, a.warmup, a.sim)
    with ThreadPoolExecutor(a.jobs) as ex:
        for n, (item, res) in enumerate(ex.map(work, todo), 1):
            (exp, pred, kb, label, env, bits), t = item
            if res is None: print('FAILED', exp, pred, label, t, flush=True); continue
            ipc, ins, cyc, mpki = res
            w.writerow([exp, pred, label, kb, os.path.basename(t), bits, ipc, mpki, ins, cyc, json.dumps(env), a.warmup, a.sim, a.tag]); f.flush()
            if n % 10 == 0: print(f'{n}/{len(todo)}', flush=True)

if __name__ == '__main__': main()
