import sys, run_experiments as r

def trimmed(exps, budgets_kb, main_kb):
    B = lambda kb: kb * 1024 * 8
    jobs = []
    for nt in [1, 2, 3, 4, 6]:
        e, b = r.size_tage(B(16), nt); jobs.append(('E2', 'tage', 16, f'nt={nt}', e, b))
    base, b = r.size_tage(B(16), 4)
    for mx in [20, 60, 130, 300, 600]:
        jobs.append(('E3a_maxh', 'tage', 16, f'maxh={mx}', {**base, 'TAGE_LENS': r.lens(r.geo(4, 5, mx))}, b))
    for mn in [2, 10, 20]:
        jobs.append(('E3b_minh', 'tage', 16, f'minh={mn}', {**base, 'TAGE_LENS': r.lens(r.geo(4, mn, 130))}, b))
    return jobs

r.build_jobs = trimmed
r.main()
