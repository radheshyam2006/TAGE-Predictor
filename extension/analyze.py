#!/usr/bin/env python3
"""Turn results.csv into tables (markdown) and plots (PNG).   python3 analyze.py results/results.csv out_dir"""
import sys, os
import pandas as pd
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt

csvp, outd = sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 'figures'
os.makedirs(outd, exist_ok=True)
d = pd.read_csv(csvp)
d['trace'] = d['trace'].str.replace(r'\.champsimtrace.*|\.trace.*', '', regex=True)
md = []

def gmean(s): return float(s.clip(lower=1e-9).pow(1 / len(s)).prod())   # geometric mean
def agg(df, by):
    g = df.groupby(by).agg(mpki=('mpki', 'mean'), ipc=('ipc', gmean), bits=('storage_bits', 'first')).reset_index()
    return g

def table(df, index, columns, values, title, fmt='{:.2f}'):
    t = df.pivot_table(index=index, columns=columns, values=values, aggfunc='mean')
    md.append(f'\n**{title}**\n\n' + t.map(lambda x: fmt.format(x)).to_markdown() + '\n')

# E1: main comparison and budget sweep
e1 = d[d.exp == 'E1']
if len(e1):
    for v, nm in [('mpki', 'MPKI (lower is better)'), ('ipc', 'IPC (higher is better)')]:
        table(e1, ['trace', 'budget_kb'], 'predictor', v, f'E1: {nm} per trace and budget (KB)', '{:.3f}')
    a = agg(e1, ['predictor', 'budget_kb'])
    md.append('\n**E1 summary: mean MPKI / geometric-mean IPC over all traces**\n\n' + a.pivot(index='budget_kb', columns='predictor', values=['mpki', 'ipc']).round(3).to_markdown() + '\n')
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    for p, g in a.groupby('predictor'):
        ax[0].plot(g.budget_kb, g.mpki, 'o-', label=p); ax[1].plot(g.budget_kb, g.ipc, 'o-', label=p)
    for x, t, yl in [(ax[0], 'Mean MPKI vs storage budget', 'MPKI'), (ax[1], 'Geomean IPC vs storage budget', 'IPC')]:
        x.set_xscale('log', base=2); x.set_xlabel('Predictor storage (KB)'); x.set_ylabel(yl); x.set_title(t); x.grid(alpha=.3); x.legend()
    plt.tight_layout(); plt.savefig(f'{outd}/e1_budget.png', dpi=150); plt.close()
    for tr, g in e1.groupby('trace'):  # per-trace bars at the middle budget
        pass
    mid = sorted(e1.budget_kb.unique())[len(e1.budget_kb.unique()) // 2]
    t = e1[e1.budget_kb == mid].pivot(index='trace', columns='predictor', values='mpki')
    t.plot.bar(figsize=(7, 4)); plt.ylabel('MPKI'); plt.title(f'Per-trace MPKI at {mid} KB'); plt.tight_layout(); plt.savefig(f'{outd}/e1_per_trace.png', dpi=150); plt.close()

def sweep(exp, xlabel, title, fname, order=None, rot=0):
    df = d[d.exp == exp]
    if not len(df): return
    a = agg(df, ['label'])
    if order: a['o'] = a.label.map({k: i for i, k in enumerate(order)}); a = a.sort_values('o')
    md.append(f'\n**{title}**\n\n' + a[['label', 'bits', 'mpki', 'ipc']].round(3).rename(columns={'bits': 'storage_bits'}).to_markdown(index=False) + '\n')
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    ax[0].plot(a.label, a.mpki, 'o-'); ax[0].set_ylabel('Mean MPKI'); ax[1].plot(a.label, a.ipc, 'o-', color='C1'); ax[1].set_ylabel('Geomean IPC')
    for x in ax: x.set_xlabel(xlabel); x.grid(alpha=.3); x.tick_params(axis='x', rotation=rot)
    fig.suptitle(title); plt.tight_layout(); plt.savefig(f'{outd}/{fname}.png', dpi=150); plt.close()
    # per-trace MPKI table for the same sweep
    table(df, 'label', 'trace', 'mpki', f'{title} -- per-trace MPKI', '{:.3f}')

sweep('E2', 'Number of tagged tables (total budget fixed)', 'E2a: number of tagged tables, equal total budget', 'e2_tables', [f'nt={n}' for n in [0, 1, 2, 3, 4, 5, 6, 8]])
sweep('E2b_fixed_tables', 'Number of tagged tables (each table same size)', 'E2b: number of tagged tables, equal table size (storage grows)', 'e2b_tables', [f'nt={n}' for n in [0, 1, 2, 3, 4, 5, 6, 8]])
sweep('E3a_maxh', 'Longest history length', 'E3a: maximum history length', 'e3a_maxh', [f'maxh={m}' for m in [20, 40, 80, 130, 260, 500, 1000]])
sweep('E3b_minh', 'Shortest history length', 'E3b: minimum history length', 'e3b_minh', [f'minh={m}' for m in [2, 3, 5, 8, 12, 20]])
sweep('E3c_shape', 'Series shape', 'E3c: geometric vs other length series', 'e3c_shape', rot=15)
sweep('E4a_tag_fixed', 'Tag width (bits), table size fixed', 'E4a: partial tag width (table size fixed, storage varies)', 'e4a_tag', [f'tag={t}' for t in [4, 6, 8, 9, 10, 12, 14]])
sweep('E0_replicate', 'Variant', 'E0: teammate variants re-run through env-var build (must match results_*.csv)', 'e0_replicate', rot=30)
sweep('E4b_policy', 'Policy variant', 'E4b: update/allocation policy ablation', 'e4b_policy', rot=20)

open(f'{outd}/tables.md', 'w').write('\n'.join(md))
print('wrote', outd)
