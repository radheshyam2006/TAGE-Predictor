import csv, collections, os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
os.makedirs("plots", exist_ok=True)
TR = ["perlbench", "gcc", "gobmk"]
KB = {"bimodal": 4, "gshare": 4, "t2": 5.5, "t3": 7.25, "t4_short": 9, "t4_default": 9, "t4_long": 9,
      "t4_small": 3.75, "t4_big": 30, "t4_huge": 114}  # approximate, see README
def load(f):
    d = collections.defaultdict(dict)
    for r in csv.DictReader(open(f)):
        d[r["variant"].replace("tage_", "")][r["trace"].split(".")[1].split("-")[0]] = (float(r["ipc"]), float(r["mpki"]))
    return d
def bars(d, idx, ylabel, names, title, out):
    fig, ax = plt.subplots(figsize=(9, 4)); w = 0.8 / len(names)
    for i, n in enumerate(names):
        ax.bar([j + i * w for j in range(3)], [d[n][t][idx] for t in TR], w, label=n)
    ax.set_xticks([j + 0.4 - w / 2 for j in range(3)]); ax.set_xticklabels(TR)
    ax.set_ylabel(ylabel); ax.set_title(title); ax.legend(fontsize=7, ncol=2); fig.tight_layout()
    fig.savefig(out, dpi=130); plt.close(fig)
for tag, f in [("p1", "results_final.csv"), ("p15", "results_p15.csv")]:
    if not os.path.exists(f): continue
    d = load(f)
    bars(d, 1, "MPKI", ["bimodal", "gshare", "t4_default"], f"MPKI: TAGE vs bimodal/gshare (penalty {tag[1:]})", f"plots/{tag}_mpki_compare.png")
    bars(d, 0, "IPC", ["bimodal", "gshare", "t4_default"], f"IPC: TAGE vs bimodal/gshare (penalty {tag[1:]})", f"plots/{tag}_ipc_compare.png")
    bars(d, 1, "MPKI", ["t2", "t3", "t4_default"], "MPKI vs number of tagged tables", f"plots/{tag}_mpki_tables.png")
    bars(d, 1, "MPKI", ["t4_short", "t4_default", "t4_long"], "MPKI vs history lengths (short 2..16, geometric 5..130, long 8..256)", f"plots/{tag}_mpki_history.png")
    fig, axs = plt.subplots(1, 2, figsize=(10, 4))
    for ax, idx, lab in [(axs[0], 1, "MPKI"), (axs[1], 0, "IPC")]:
        for t in TR:
            xs = [KB[n] for n in ["t4_small", "t4_default", "t4_big", "t4_huge"]]
            ax.plot(xs, [d[n][t][idx] for n in ["t4_small", "t4_default", "t4_big", "t4_huge"]], "o-", label=t)
            ax.axhline(d["gshare"][t][idx], ls=":", color="gray", lw=0.7)
        ax.set_xscale("log"); ax.set_xlabel("approx. storage (KB)"); ax.set_ylabel(lab); ax.legend()
    fig.suptitle(f"Storage budget sweep (dotted = gshare, 4KB), penalty {tag[1:]}"); fig.tight_layout()
    fig.savefig(f"plots/{tag}_storage.png", dpi=130); plt.close(fig)
print(sorted(os.listdir("plots")))
