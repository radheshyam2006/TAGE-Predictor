# Simplified TAGE branch predictor in ChampSim

Implementation of a simplified **TAGE** predictor (base predictor + 4 tagged tables) in ChampSim's branch-predictor
interface, compared with **bimodal** and **gshare** on three SPEC CPU2006 traces. We measure how the number of tables,
the history lengths and the total storage affect **MPKI** (branch mispredictions per 1000 instructions, lower is better)
and **IPC** (instructions per cycle, higher is better).

Reference: Seznec & Michaud, *A case for (partially) tagged geometric history length branch prediction*, JILP 2006
(primary); Seznec, *A new case for the TAGE branch predictor*, MICRO 2011 (supplementary).

## 1. The predictor (`branch/tage/`)
| Part | Choice |
|---|---|
| Base predictor T0 | Bimodal table, 2-bit counters, indexed by PC (default 8192 entries) |
| Tagged tables T1..T4 | 1024 entries each by default; entry = 9-bit partial tag + 3-bit signed counter + 2-bit useful counter `u` |
| History lengths | Geometric series 5, 15, 44, 130 (global branch history, XOR-folded to index and tag width) |
| Prediction | The matching table with the longest history is the *provider*; if no tag matches the base predictor answers. The next match (or the base) is the *alternate* |
| Update | The provider's counter moves toward the outcome. `u` goes up when the provider is right and the alternate wrong, down in the opposite case |
| Allocation | On a misprediction, take the first longer-history table whose entry has `u = 0` and write a new entry (weak counter, `u = 0`). If none is free, decrement `u` of the candidates |
| Aging | All `u` counters are halved every 2^18 updates |

Not implemented (they are in the papers): use-alternate-on-newly-allocated, several allocations per misprediction,
path history, loop predictor, statistical corrector.

Every parameter can be set by an environment variable (`TAGE_NT`, `TAGE_LENS`, `TAGE_LOGT`, `TAGE_TAG`, `TAGE_LOGBASE`,
`TAGE_UPERIOD`), so one binary runs all experiments. Bimodal and gshare (`branch/bimodalp`, `branch/gsharep`) are sized by
`BP_LOGSIZE` and `GS_LOGSIZE` so they can be compared at the same storage as TAGE.

## 2. Layout
```
branch/tage, bimodalp, gsharep   the three predictors (copied into ChampSim by setup.sh)
setup.sh                         installs them into ChampSim and builds three binaries
run_experiments.py               runs the experiments, writes results/results.csv (resumable)
make_figures.py                  draws results/figures/*.png from the CSV
results/                         results.csv and the three figures
original_version/                the first version of this project (kept unchanged for reference)
```

## 3. Build and run (Linux / WSL)
ChampSim commit used: `5f0be14a12f81baec35583dd4721afa654e93981`.
```bash
git clone --recursive https://github.com/ChampSim/ChampSim.git ~/ChampSim
cd ~/ChampSim && git checkout 5f0be14a12f81baec35583dd4721afa654e93981 && git submodule update --init
vcpkg/bootstrap-vcpkg.sh && vcpkg/vcpkg install
cd <this repo> && ./setup.sh ~/ChampSim            # builds champsim_tage / _bimodalp / _gsharep (mispredict penalty 15)
# traces: 400.perlbench-41B, 403.gcc-16B, 445.gobmk-17B from https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/ into ~/traces
python3 run_experiments.py --champsim ~/ChampSim/bin --traces ~/traces --warmup 5000000 --sim 20000000 --exp E1,E2,E3 --jobs 6 --out results/results.csv
python3 make_figures.py
```
Use `--exp E0` to re-run the 8 variants of the original version as a check.

## 4. Results
Setup: 5M warmup + 20M simulated instructions per run, mispredict penalty 15 cycles, each setting run once.
Storage counts only the predictor tables (counters, tags, `u`).

### 4.1 Equal storage (E1) -- `results/figures/fig1_storage.png`
Average over the three traces:

| Storage | bimodal MPKI | gshare MPKI | TAGE MPKI | bimodal IPC | gshare IPC | TAGE IPC |
|---|---|---|---|---|---|---|
| 4 KB | 12.05 | 10.61 | 8.11 | 1.238 | 1.280 | 1.340 |
| 8 KB | 11.93 | 9.15 | 6.79 | 1.240 | 1.314 | 1.379 |
| 16 KB | 11.91 | 8.13 | 5.87 | 1.239 | 1.342 | 1.410 |
| 32 KB | 11.84 | 7.37 | 5.20 | 1.242 | 1.365 | 1.437 |
| 64 KB | 11.84 | 6.82 | 4.74 | 1.242 | 1.383 | 1.458 |

Per trace, MPKI:

| Trace | Storage | bimodal | gshare | TAGE |
|---|---|---|---|---|
| perlbench | 4 KB | 10.430 | 8.893 | **5.759** |
| perlbench | 8 KB | 10.550 | 8.001 | **4.993** |
| perlbench | 16 KB | 10.410 | 7.378 | **4.685** |
| perlbench | 32 KB | 10.270 | 7.034 | **4.551** |
| perlbench | 64 KB | 10.280 | 6.682 | **4.473** |
| gcc | 4 KB | 0.651 | 0.373 | **0.188** |
| gcc | 8 KB | 0.650 | 0.328 | **0.164** |
| gcc | 16 KB | 0.691 | 0.281 | **0.158** |
| gcc | 32 KB | 0.651 | 0.268 | **0.156** |
| gcc | 64 KB | 0.650 | 0.256 | **0.155** |
| gobmk | 4 KB | 25.060 | 22.550 | **18.370** |
| gobmk | 8 KB | 24.590 | 19.110 | **15.200** |
| gobmk | 16 KB | 24.630 | 16.730 | **12.780** |
| gobmk | 32 KB | 24.590 | 14.810 | **10.900** |
| gobmk | 64 KB | 24.600 | 13.510 | **9.587** |

- TAGE has the lowest MPKI at every size on every trace.
- TAGE reaches gshare's accuracy with less storage: at 8 KB it beats gshare at 64 KB on perlbench and gcc; on gobmk it takes 16 KB.
- Bimodal barely changes with size. gcc is very easy for all predictors (below 0.7 MPKI), so its IPC hardly moves.

### 4.2 Number of tagged tables (E2, 16 KB) -- `fig2_tables.png`
| Tagged tables | perlbench | gcc | gobmk |
|---|---|---|---|
| nt=1 | 8.786 | 0.456 | 19.39 |
| nt=2 | 5.497 | 0.199 | 17.42 |
| nt=3 | 4.841 | 0.167 | 14.59 |
| nt=4 | 4.685 | 0.158 | 12.78 |
| nt=6 | 4.783 | 0.162 | 12.91 |

MPKI falls from 1 to 4 tables on all three traces and does not improve at 6. The 3- and 6-table runs use about 12.5 KB of the 16 KB because table sizes are powers of two.

### 4.3 History lengths (E3, 4 tables, 16 KB) -- `fig3_history.png`
| Longest history (shortest = 5) | perlbench | gcc | gobmk |
|---|---|---|---|
| maxh=20 | 5.886 | 0.233 | 12.63 |
| maxh=60 | 5.025 | 0.167 | 11.91 |
| maxh=130 | 4.685 | 0.158 | 12.78 |
| maxh=300 | 4.702 | 0.158 | 13.82 |
| maxh=600 | 4.809 | 0.166 | 14.78 |

| Shortest history (longest = 130) | perlbench | gcc | gobmk |
|---|---|---|---|
| minh=2 | 4.729 | 0.179 | 13.51 |
| minh=10 | 4.713 | 0.159 | 12.52 |
| minh=20 | 4.897 | 0.166 | 14.50 |

The default (5 and 130) is best or within about 1% on perlbench and gcc. gobmk does better with a shorter longest length (60) and a shortest length of 10. A shortest length of 20 is worse than the default on all three traces.

## 5. Checks done
- With default settings the runtime-configurable predictor gives results identical to the original version: all 24 TAGE rows (8 variants x 3 traces) match `original_version/results_p15.csv` on the real traces (experiment E0).
- The storage counter in the code reports 73,728 bits (9 KB) for the default configuration, matching the hand estimate.
- The same configuration (4 tables, 5/15/44/130, 16 KB) gives the same MPKI in E1, E2 and E3.

## 6. Limitations
- Only three SPEC CPU2006 integer traces and 20M instructions per run; each setting was run once, so differences of a few percent should not be ranked.
- The bimodal and gshare used here are our own size-adjustable versions: they learn only from conditional branches and hash slightly differently from ChampSim's built-in ones, so their numbers differ from those in `original_version/`. TAGE (like the built-ins) also learns from jumps, calls and returns.
- Storage excludes the history registers, for all predictors.
- IPC depends on the mispredict penalty (15 here; `original_version/` also has runs with 1).

## 7. Credits
The first version (predictor, 8-variant experiments, plots) is in `original_version/` (original README inside). The current version adds
runtime-configurable parameters, equal-storage baselines and the experiments in section 4.
