#!/usr/bin/env python3
"""Generate labelled evidence plots from the committed CSV results.

Run from anywhere: python3 scripts/generate_plots.py
Reads results/csv/*.csv and results/benchmarks/*.csv (already-committed
raw evidence -- this script only visualizes it, never invents data) and
writes labelled PNGs to results/plots/. Requires matplotlib
(`pip install matplotlib` or `pip install --break-system-packages matplotlib`).
"""
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_DIR = os.path.join(REPO_ROOT, "results", "csv")
BENCH_DIR = os.path.join(REPO_ROOT, "results", "benchmarks")
OUT_DIR = os.path.join(REPO_ROOT, "results", "plots")
os.makedirs(OUT_DIR, exist_ok=True)


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def savefig(fig, name):
    path = os.path.join(OUT_DIR, name)
    fig.savefig(path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print("wrote", path)


# ---------------------------------------------------------------------
# 1. Thermal: FE vs exact temperature profile (N=40), highlighting
#    midpoint interpolation error is where discretization error lives.
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(CSV_DIR, "thermal_n40.csv"))
node_x = [float(r["x_m"]) for r in rows if r["kind"] == "node"]
node_fe = [float(r["temperature_fe_C"]) for r in rows if r["kind"] == "node"]
node_exact = [float(r["temperature_exact_C"]) for r in rows if r["kind"] == "node"]
mid_x = [float(r["x_m"]) for r in rows if r["kind"] == "midpoint"]
mid_err = [float(r["abs_error_C"]) for r in rows if r["kind"] == "midpoint"]

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7, 7), sharex=True)
ax1.plot(node_x, node_exact, "-", color="#333333", linewidth=1.5, label="Exact T(x)")
ax1.plot(node_x, node_fe, "o", color="#2b6cb0", markersize=3, label="FE nodal T (N=40)")
ax1.set_ylabel("Temperature (°C)")
ax1.set_title("Thermal FEA: FE solution vs. exact solution (N=40 elements)")
ax1.legend()
ax1.grid(alpha=0.3)

ax2.plot(mid_x, mid_err, "-", color="#c53030", linewidth=1.2)
ax2.set_xlabel("x (m)")
ax2.set_ylabel("Midpoint abs. error (°C)")
ax2.set_title("FE interpolation error at element midpoints (nodes are exact; error lives between them)")
ax2.grid(alpha=0.3)
savefig(fig, "thermal_profile_and_midpoint_error_n40.png")

# ---------------------------------------------------------------------
# 2. Thermal: mesh convergence (log-log, slope should be ~2 for O(h^2))
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(CSV_DIR, "thermal_convergence.csv"))
h = [float(r["element_size_h_m"]) for r in rows]
err = [float(r["max_abs_midpoint_error_C"]) for r in rows]

fig, ax = plt.subplots(figsize=(6, 5))
ax.loglog(h, err, "o-", color="#2b6cb0", label="Measured max midpoint error")
# reference O(h^2) line through the first point
ref = [err[0] * (hv / h[0]) ** 2 for hv in h]
ax.loglog(h, ref, "--", color="#999999", label="O(h²) reference")
ax.set_xlabel("Element size h (m)")
ax.set_ylabel("Max abs. midpoint error (°C)")
ax.set_title("Thermal FEA mesh convergence (N=10,20,40,80)")
ax.legend()
ax.grid(alpha=0.3, which="both")
savefig(fig, "thermal_mesh_convergence.png")

# ---------------------------------------------------------------------
# 3. Neural: training loss curve
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(CSV_DIR, "neural_training.csv"))
steps = [int(r["step"]) for r in rows]
loss = [float(r["loss"]) for r in rows]

fig, ax = plt.subplots(figsize=(6, 4.5))
ax.plot(steps, loss, color="#2b6cb0", linewidth=1.3)
ax.set_xlabel("Training step")
ax.set_ylabel("Loss L = 0.5(ŷ - y)²")
ax.set_title("Neural Computation: hand-implemented backprop, 100-step gradient descent")
ax.grid(alpha=0.3)
savefig(fig, "neural_training_loss.png")

# ---------------------------------------------------------------------
# 4. Neural: analytic vs finite-difference gradient check (bar chart)
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(CSV_DIR, "neural_gradient_check.csv"))
params = [r["param"] for r in rows]
analytic = [float(r["analytic"]) for r in rows]
fd = [float(r["finite_difference"]) for r in rows]

fig, ax = plt.subplots(figsize=(6, 4.5))
x = range(len(params))
width = 0.35
ax.bar([i - width / 2 for i in x], analytic, width, label="Analytic (backprop)", color="#2b6cb0")
ax.bar([i + width / 2 for i in x], fd, width, label="Central finite-difference", color="#c53030", alpha=0.75)
ax.set_xticks(list(x))
ax.set_xticklabels(params)
ax.set_ylabel("Gradient value")
ax.set_title("Neural Computation: analytic vs. finite-difference gradient check")
ax.legend()
ax.grid(alpha=0.3, axis="y")
savefig(fig, "neural_gradient_check.png")

# ---------------------------------------------------------------------
# 5. Performance: speedup / efficiency vs worker count
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(BENCH_DIR, "performance_summary.csv"))
workers = [int(r["num_workers"]) for r in rows]
speedup = [float(r["speedup"]) for r in rows]
efficiency = [float(r["efficiency"]) * 100 for r in rows]
median_s = [float(r["median_seconds"]) for r in rows]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4.5))
ax1.plot(workers, median_s, "o-", color="#2b6cb0")
ax1.set_xlabel("Worker count")
ax1.set_ylabel("Median wall time (s)")
ax1.set_title("Performance Lab: wall time vs. worker count\n(4000 cases × N=200 elements)")
ax1.set_xticks(workers)
ax1.grid(alpha=0.3)

xpos = list(range(len(workers)))
bar1 = ax2.bar([p - 0.17 for p in xpos], speedup, width=0.34, label="Speedup (×)", color="#2b6cb0")
ax2b = ax2.twinx()
bar2 = ax2b.bar([p + 0.17 for p in xpos], efficiency, width=0.34, label="Efficiency (%)", color="#dd6b20")
ax2.set_xticks(xpos)
ax2.set_xticklabels([str(w) for w in workers])
ax2.set_xlabel("Worker count")
ax2.set_ylabel("Speedup (×, vs. 1-worker baseline)")
ax2b.set_ylabel("Parallel efficiency (%)")
ax2.set_ylim(0, max(speedup) * 1.35 if max(speedup) > 0 else 1)
ax2b.set_ylim(0, max(efficiency) * 1.35 if max(efficiency) > 0 else 100)
ax2.set_title("Speedup and efficiency\n(this container: 2 logical CPUs)")
ax2.legend(handles=[bar1, bar2], labels=["Speedup (×)", "Efficiency (%)"], loc="upper left", fontsize=8)
fig.tight_layout()
savefig(fig, "performance_speedup_efficiency.png")

# ---------------------------------------------------------------------
# 6. Hardware: telemetry trace with the scripted dropout/recovery event
# ---------------------------------------------------------------------
rows = read_csv(os.path.join(CSV_DIR, "hardware_telemetry_mcu-sim-01.csv"))
t = [float(r["timestamp_s"]) for r in rows]
volt = [float(r["voltage_v"]) if r["voltage_v"] else None for r in rows]
temp = [float(r["temperature_c"]) if r["temperature_c"] else None for r in rows]
locked = [r["clock_locked"] == "true" for r in rows]
dropout_t = [tt for tt, lk in zip(t, locked) if not lk]

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7, 6), sharex=True)
ax1.plot(t, volt, "-o", color="#2b6cb0", markersize=3, label="Voltage (V)")
for dt in dropout_t:
    ax1.axvline(dt, color="#c53030", linestyle="--", alpha=0.7, label="Simulated dropout" if dt == dropout_t[0] else None)
ax1.set_ylabel("Voltage (V)")
ax1.set_title("Hardware Integration (mock mcu-sim-01): telemetry with simulated dropout + recovery")
ax1.legend()
ax1.grid(alpha=0.3)

ax2.plot(t, temp, "-o", color="#dd6b20", markersize=3, label="Temperature (°C)")
for dt in dropout_t:
    ax2.axvline(dt, color="#c53030", linestyle="--", alpha=0.7)
ax2.set_xlabel("Simulated session time (s)")
ax2.set_ylabel("Temperature (°C)")
ax2.grid(alpha=0.3)
savefig(fig, "hardware_telemetry_dropout_recovery.png")

print("done")
