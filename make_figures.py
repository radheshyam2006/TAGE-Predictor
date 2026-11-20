# Draws the three figures (storage budget, number of tables, history lengths) from results/results.csv
import pandas as pd, matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
d = pd.read_csv('results/results.csv')
d['trace'] = d.trace.str.split('-').str[0].str.replace(r'^\d+\.', '', regex=True)
TR = ['perlbench', 'gcc', 'gobmk']; COL = {'bimodal': 'tab:gray', 'gshare': 'tab:blue', 'tage': 'tab:red'}
LAB = {'bimodal': 'bimodal', 'gshare': 'gshare', 'tage': 'TAGE (4 tables)'}

# Fig 1: storage budget, MPKI (top) and IPC (bottom) per trace
e1 = d[d.exp == 'E1']
fig, ax = plt.subplots(2, 3, figsize=(11, 6))
for j, tr in enumerate(TR):
    for p in ['bimodal', 'gshare', 'tage']:
        g = e1[(e1.trace == tr) & (e1.predictor == p)].sort_values('budget_kb')
        ax[0, j].plot(g.budget_kb, g.mpki, 'o-', color=COL[p], label=LAB[p]); ax[1, j].plot(g.budget_kb, g.ipc, 'o-', color=COL[p])
    ax[0, j].set_title(tr); ax[0, j].set_ylabel('MPKI (lower = better)' if j == 0 else ''); ax[1, j].set_ylabel('IPC (higher = better)' if j == 0 else '')
    for i in (0, 1): ax[i, j].set_xscale('log', base=2); ax[i, j].set_xticks([4, 8, 16, 32, 64]); ax[i, j].set_xticklabels([4, 8, 16, 32, 64]); ax[i, j].grid(alpha=.3)
    ax[1, j].set_xlabel('Predictor storage (KB)')
ax[0, 0].legend(fontsize=8); fig.suptitle('Fig 1: bimodal vs gshare vs TAGE at equal storage'); plt.tight_layout(); plt.savefig('results/figures/fig1_storage.png', dpi=160); plt.close()

# Fig 2: number of tagged tables (16 KB total)
e2 = d[d.exp == 'E2']; e2 = e2.assign(nt=e2.label.str.replace('nt=', '').astype(int))
fig, ax = plt.subplots(1, 3, figsize=(11, 3.4))
for j, tr in enumerate(TR):
    g = e2[e2.trace == tr].sort_values('nt'); ax[j].plot(g.nt, g.mpki, 'o-', color='tab:red')
    ax[j].set_title(tr); ax[j].set_xlabel('Number of tagged tables'); ax[j].set_ylabel('MPKI' if j == 0 else ''); ax[j].grid(alpha=.3)
fig.suptitle('Fig 2: number of tagged tables (TAGE, 16 KB budget)'); plt.tight_layout(); plt.savefig('results/figures/fig2_tables.png', dpi=160); plt.close()

# Fig 3: history lengths
mx = d[d.exp == 'E3a_maxh']; mx = mx.assign(v=mx.label.str.replace('maxh=', '').astype(int))
mn = d[d.exp == 'E3b_minh']; mn = mn.assign(v=mn.label.str.replace('minh=', '').astype(int))
base = e2[e2.nt == 4].assign(v=5)   # default shortest length 5 = same run as maxh=130
mn = pd.concat([mn, base[mn.columns]])
fig, ax = plt.subplots(2, 3, figsize=(11, 6))
for j, tr in enumerate(TR):
    g = mx[mx.trace == tr].sort_values('v'); ax[0, j].plot(g.v, g.mpki, 'o-', color='tab:red'); ax[0, j].set_xscale('log'); ax[0, j].set_xlabel('Longest history length')
    h = mn[mn.trace == tr].sort_values('v'); ax[1, j].plot(h.v, h.mpki, 'o-', color='tab:green'); ax[1, j].set_xscale('log'); ax[1, j].set_xlabel('Shortest history length')
    ax[0, j].set_title(tr)
    for i in (0, 1): ax[i, j].set_ylabel('MPKI' if j == 0 else ''); ax[i, j].grid(alpha=.3)
fig.suptitle('Fig 3: history lengths (4 tables, 16 KB; defaults 5 and 130)'); plt.tight_layout(); plt.savefig('results/figures/fig3_history.png', dpi=160); plt.close()
print('ok')
