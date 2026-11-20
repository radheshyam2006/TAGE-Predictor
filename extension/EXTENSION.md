# Extension to the TAGE project (what was added on top of the teammate's work)

Nothing the teammate did is replaced. `tage_module/` is his predictor made runtime-configurable; with no
environment variables set it behaves **bit-identically** to his original (verified, see below).

## What changed
| File | Change |
|---|---|
| `tage_module/tage.h/.cc` | Parameters (tables, lengths, sizes, tag bits) readable from env vars so ONE binary runs every experiment (his `-D` macros still work as defaults). History folding made incremental (same function, much faster for long histories). Added optional switches, all OFF by default: `TAGE_USEALT` (use-alternate-on-newly-allocated), `TAGE_NALLOC` (allocate several entries), `TAGE_UPERIOD` (aging period, 0=off), `TAGE_CONDONLY` (train on conditional branches only). Added `storage_bits()` and a stats dump (`TAGE_STATS_FILE`): real storage, allocation successes/failures, aging count, provider usage per table. |
| `branch/bimodalp`, `branch/gsharep` | Bimodal / gshare whose size is set by env var (`BP_LOGSIZE`, `GS_LOGSIZE`), so they can be compared at the SAME storage as TAGE. |
| `run_experiments.py` | Runs all experiments over a trace folder, writes one CSV (resumable). |
| `analyze.py` | CSV -> markdown tables + plots. |
| `setup_extension.sh` | Installs the modules into a ChampSim checkout and builds the 3 binaries (`PEN=15` mispredict penalty by default). |

## Experiments (they cover the gaps listed in the audit)
- **E0** re-runs his 8 variants through the new build: a check that must reproduce `results_final.csv` / `results_p15.csv`.
- **E1** bimodal, gshare, TAGE at equal storage: 4, 8, 16, 32, 64 KB (the missing fair comparison and budget sweep).
- **E2** 0-8 tagged tables, same geometric series resampled (no longer confounded); equal total budget, and equal table size.
- **E3** max history 20-1000, min history 2-20, series shape.
- **E4** tag width 4-14 bits; policy ablations (no aging, use-alt, 2-entry allocation, conditional-only training).

## Verification done (synthetic traces only, no real traces available to the author)
- Original vs new build, MPKI and IPC identical for: default, 2 tables (8,44), bigger tables (4096 entries, 11-bit tags), on 2 traces.
- New `storage_bits()` = 73,728 bits = 9 KB for the default, matching his hand estimate; all six estimates in his README reproduce.
- E0 through `run_experiments.py` gives the same MPKI as the direct runs (23.63 and 33.58 on the test trace).

## Run it (WSL, after ChampSim is built)
```bash
cp -r this_folder ~/ext && cd ~/ext
PEN=15 ./setup_extension.sh ~/ChampSim            # builds champsim_tage, champsim_bimodalp, champsim_gsharep
python3 run_experiments.py --champsim ~/ChampSim/bin --traces ~/traces --warmup 5000000 --sim 20000000 \
        --exp E0,E1 --jobs $(nproc) --out results/results.csv      # start with E0+E1
python3 run_experiments.py ... --exp E2,E3,E4                      # then the rest (same --out; it resumes)
python3 analyze.py results/results.csv figures
```
E0's numbers should equal his `results_p15.csv` (MPKI exactly; IPC if the same penalty and lengths are used).
Rough cost (my estimate): about 2 minutes per run per core at 25M instructions; ~210 runs for 3 traces.

## Versions and how the final results were produced
- ChampSim commit: `5f0be14a12f81baec35583dd4721afa654e93981`
- Traces: 400.perlbench-41B, 403.gcc-16B, 445.gobmk-17B (SPEC CPU2006, DPC-3), 5M warmup + 20M simulated instructions, mispredict penalty 15.
- Results file: `extension/results/results.csv`
  - `E0_replicate`: the teammate's 8 TAGE variants re-run; MPKI and IPC match his `results_p15.csv` exactly (a check that the modified predictor behaves identically).
  - `E1`: bimodal, gshare, TAGE at equal storage, 4 to 64 KB.
  - `E2`, `E3a_maxh`, `E3b_minh`: number of tables, longest and shortest history length (`run_required.py`).
- The bimodal and gshare used in E1 are re-written, size-adjustable versions (`extension/branch/`), not ChampSim's built-in ones, so their numbers differ from the E0 rows for the built-in versions.
