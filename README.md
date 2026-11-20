# TAGE Branch Predictor in ChampSim: Project README

## 1. What this project is

A CPU must guess which way a branch (`if`, loop, ...) will go *before* it knows, so it can keep fetching instructions. A wrong guess (misprediction) flushes the pipeline and wastes cycles.

This project:
1. Implements a **simplified TAGE** branch predictor inside **ChampSim** (a CPU simulator).
2. Compares it to two classic predictors, **bimodal** and **gshare**, on real program traces.
3. Studies how the **number of tables**, **history lengths** and **storage size** change
   - **MPKI** = mispredictions per 1000 instructions (lower is better)
   - **IPC** = instructions per cycle (higher is better)

## 2. What was installed / downloaded, and why

| Thing | Why |
|---|---|
| **ChampSim** (`ChampSim/`, cloned from GitHub) | The trace-driven CPU simulator. It models the pipeline, caches and branch predictors, and replays a recorded instruction trace. |
| **vcpkg** (git submodule, `ChampSim/vcpkg`) and the libraries it installed (`vcpkg_installed/`: CLI11, fmt, zlib, lzma, bzip2) | ChampSim's dependencies. They are used for command-line parsing, text formatting, and reading compressed traces. |
| `g++`, `make`, `cmake` (already on the system) | To compile ChampSim. No sudo was needed. |
| **3 SPEC CPU2006 traces** (`traces/`, from the DPC-3 site): `400.perlbench-41B`, `403.gcc-16B`, `445.gobmk-17B` (about 1 GB total) | The workload. A trace is a recorded stream of instructions, including every branch and its outcome. These three are branch-heavy integer programs. |

Build flow: `./config.sh champsim_config.json` generates the build setup for a chosen config (CPU parameters plus the branch predictor name). `make` then produces `bin/<executable_name>`.

## 3. Background you need

**Bimodal** is the simplest predictor. It keeps a table of 2-bit saturating counters indexed by the branch address (PC). A branch that was mostly taken recently is predicted taken. It cannot see patterns across branches.

**gshare** XORs the PC with the **global history**, a shift register of the last N branch outcomes (1 = taken). It captures correlation between branches ("if the last 3 branches were T,N,T then this one is taken"). The downside is that unrelated branches can collide on the same entry (aliasing).

**TAGE** (TAgged GEometric history length) is the state-of-the-art idea behind most modern predictors. Its three key ideas:

1. **Geometric history lengths.** There are several tables, each indexed by a hash of PC and a *different length* of global history, for example 5, 15, 44 and 130 branches. Lengths grow roughly geometrically (each is about 3x the last). Short histories learn quickly and handle easy branches. Long histories catch rare, deeply correlated patterns. A geometric series covers both with few tables.
2. **Partial tags.** Each entry stores a small tag (here 9 bits) taken from a hash of PC and history. A lookup counts as a "hit" only if the tag matches. This avoids the aliasing problem of gshare, because an entry is only used if it probably belongs to this branch and history.
3. **Longest match wins.** On each prediction, all tables are checked. The hitting table with the **longest history** (the *provider*) makes the prediction. If nothing hits, the **base bimodal** predictor is used. The next-longest hit is the *alternate* prediction (*altpred*).

**Update and allocation policy** (the core of the algorithm):
- **Counter update:** the provider's 3-bit counter moves toward the actual outcome. If there was no provider, the base counter is updated.
- **Useful bit (`u`, 2 bits):** an entry's `u` is incremented when it was right and the alternate was wrong, so it is worth keeping. It is decremented if it was wrong where the alternate was right.
- **Allocation on misprediction:** if the prediction was wrong, we try to create a new entry in a table with a **longer history** than the provider. We look for an entry with `u == 0` and overwrite it with the new tag, initialising the counter weakly toward the actual outcome. The reasoning is that if a short history failed, a longer one might distinguish the case.
- **No free slot:** if every longer table has `u > 0`, we decrement their `u` values (so slots eventually free up).
- **Aging:** every 256K updates, all `u` bits are halved, so stale entries don't block allocation forever.

## 4. What the code does

Everything is in `ChampSim/branch/tage/`:
- `tage.h`: the data structures and compile-time parameters.
  - `TAGE_NT` is the number of tagged tables (default 4).
  - `TAGE_HL` is the history lengths (default `5,15,44,130`).
  - `TAGE_LOGSZ` is log2 of entries per table (default 10, so 1024).
  - `TAGE_TAGBITS` is the tag width (default 9).
  - `TAGE_LOGBASE` is log2 of base table size (default 13).
  - Each entry holds `tag` (9 bits), `ctr` (3 bits) and `u` (2 bits).
- `tage.cc`:
  - `fold()` compresses the first `len` history bits into a few bits by XOR-folding, so a 130-bit history can index a 1024-entry table.
  - `predict_branch()` computes each table's index and tag, finds the provider and alternate, and returns the provider's prediction (or the base prediction).
  - `last_branch_result()` is called when the real outcome is known. It runs allocation, updates the useful bits and counters, ages the `u` bits, and shifts the outcome into the global history.

ChampSim calls these two functions through its branch-predictor interface (same as `branch/gshare/`). The predictor is selected by `"branch_predictor": "tage"` in `champsim_config.json`.

## 5. The experiment scripts

- `run_experiments.sh` builds bimodal, gshare and 8 TAGE variants (passing `-DTAGE_...` flags to recompile) and runs each on all 3 traces (5M warmup plus 20M simulated instructions).
  - **Known bug:** it parsed the *warmup* IPC instead of the final IPC, so the IPC column in `results.csv` / `run.log` is wrong. Those two files are obsolete. MPKI in them is fine.
- `rerun_parallel.sh` re-runs the built binaries in parallel with correct parsing and writes **`results_final.csv`**, the file to use.

TAGE variants tested:

| Name | Tables | History lengths | Entries/table | Approx. storage* |
|---|---|---|---|---|
| t4_default | 4 | 5,15,44,130 | 1024 | about 9 KB |
| t2 | 2 | 8,44 | 1024 | about 5.5 KB |
| t3 | 3 | 5,20,80 | 1024 | about 7 KB |
| t4_short | 4 | 2,4,8,16 | 1024 | about 9 KB |
| t4_long | 4 | 8,32,128,256 | 1024 | about 9 KB |
| t4_small | 4 | 5,15,44,130 | 256 | about 3.75 KB |
| t4_big | 4 | 5,15,44,130 | 4096 | about 30 KB |
| t4_huge | 4 | 5,15,44,130 | 16384 | about 114 KB |

*Storage = tables x entries x 14 bits, plus the base table (8192 x 2 bits = 2 KB). These are my own hand estimates and are not computed by the code. gshare uses 16384 x 2 bits = 4 KB. Bimodal's size was not checked.

## 6. Results (`results_final.csv`)

Format: MPKI / IPC.

| Predictor | perlbench | gcc | gobmk |
|---|---|---|---|
| bimodal | 10.57 / 0.802 | 0.65 / 2.216 | 24.76 / 1.134 |
| gshare | 11.38 / 0.804 | 0.48 / 2.223 | 28.52 / 1.088 |
| **TAGE default** | **4.89 / 0.880** | **0.16 / 2.255** | **14.88 / 1.364** |

Findings:
- TAGE cuts MPKI by roughly 40-75% versus bimodal and gshare, and IPC rises by about 2-22%.
- **More tables help**, with diminishing returns: 2 → 3 → 4 tables gives gobmk MPKI 16.6 → 15.6 → 14.9.
- **Geometric lengths beat both extremes.** Too short (2..16) and too long (8..256) are both worse than 5..130.
- **More storage helps** (gobmk: 19.3 → 14.9 → 10.9 → 8.8 MPKI as entries go 256 → 16384). gcc saturates early, so it only needs a small table.
- IPC tracks MPKI, but the IPC gain is modest because the config uses `mispredict_penalty: 1`, which is unrealistically low. Real CPUs lose roughly 15-20 cycles per misprediction.

## 7. Is the assignment done?

**Mostly yes.** Done:
- Simplified TAGE (base plus 4 tagged tables) is implemented with geometric history, partial tags, allocation and update.
- It was compared with bimodal and gshare on 3 traces.
- Number of tables, history lengths and storage were varied, and the effect on MPKI and IPC was measured.

**Not done / honest gaps**
- **No written report or plots.** The deliverable probably wants a report, and the table and findings above can seed it. I can generate charts from `results_final.csv` if you want.
- **Storage is not measured in code** and the table above is an estimate. I'd also vary storage at *fixed* tables, which was done (entries/table), but not total budget swept in KB against gshare/bimodal at the *same* size, which is a fairer comparison.
- **Mispredict penalty is 1.** For more meaningful IPC, re-run with about 15-20.
- **Simplifications vs. the paper:** no "use alternate on newly allocated entry" rule, a single allocation per misprediction, naive history folding (slow but correct), and no statistical corrector or loop predictor (these are in the 2011 paper). Entries are initialised with tag 0, so there are a few false hits early on.
- Only 3 traces and 20M instructions, so the trends are solid but the numbers are approximate.
- I did not read the two papers for you. Reading the first (2006) paper's Sections 2-3 would let you explain the design in a viva.

## 8. What to learn (short list)
1. Branch prediction basics: 2-bit saturating counters, global history, aliasing.
2. How bimodal and gshare work (the ChampSim sources in `ChampSim/branch/` are short).
3. TAGE: the three ideas in section 3, plus the allocation and useful-bit policy. Read the 2006 paper (Sections 2-3), then skim the 2011 paper for the extras.
4. Metrics: MPKI vs IPC, and why mispredict penalty links them.
5. ChampSim basics: config JSON, building, trace format, warmup vs simulation.

## 9. How to reproduce
```bash
cd ChampSim
./config.sh champsim_config.json && make -j8
bin/champsim --warmup-instructions 5000000 --simulation-instructions 20000000 ../traces/400.perlbench-41B.champsimtrace.xz
# full sweep:
cd .. && ./run_experiments.sh      # builds variants (slow)
./rerun_parallel.sh                # correct IPC -> results_final.csv
```

## 10. Realistic mispredict penalty (15 cycles): `results_p15.csv`
Rebuilt every variant with `mispredict_penalty = 15` (`rerun_penalty.sh`). IPC now separates the predictors clearly:

| Predictor | perlbench IPC | gcc IPC | gobmk IPC |
|---|---|---|---|
| bimodal | 0.720 | 2.177 | 0.819 |
| gshare | 0.717 | 2.195 | 0.763 |
| TAGE default | 0.832 (+16%) | 2.245 (+3%) | 1.071 (+31%) |
| TAGE 114 KB (t4_huge) | 0.843 | 2.246 | 1.329 (+62%) |

Plots are in `plots/`: `p1_*` is penalty 1 and `p15_*` is penalty 15. They cover predictor comparison (MPKI and IPC), number of tables, history lengths, and the storage sweep.
`make_plots.py` regenerates them.

## 11. Repo layout and setup (for teammates)
The repo holds only our work. ChampSim itself and the traces are not stored here.
- `tage_module/`: `tage.h`, `tage.cc` and `champsim_config.json`.
- scripts, `results_*.csv` (`results.csv` has a wrong IPC column, ignore it), `plots/`, this README.

Setup:
```bash
git clone https://github.com/ChampSim/ChampSim && cd ChampSim
git submodule update --init && vcpkg/bootstrap-vcpkg.sh && vcpkg/vcpkg install
mkdir branch/tage && cp ../tage_module/tage.* branch/tage/ && cp ../tage_module/champsim_config.json .
./config.sh champsim_config.json && make -j8
```
Download the traces from https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/ into `../traces/`.
The scripts expect the `ChampSim/` and `traces/` folders to sit beside them.
