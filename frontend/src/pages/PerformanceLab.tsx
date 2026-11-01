import { useEffect, useState } from "react";
import { fetchBenchmark, ApiError } from "../services/api";
import type { BenchmarkReportDto } from "../types/performance";
import Plot from "../components/Plot";

type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; data: BenchmarkReportDto };

const PLOT_LAYOUT_BASE = {
  paper_bgcolor: "#111726",
  plot_bgcolor: "#111726",
  font: { color: "#e2e8f0", size: 12 },
  margin: { l: 55, r: 20, t: 10, b: 45 },
  xaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
  yaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
};

export default function PerformanceLab() {
  const [state, setState] = useState<LoadState>({ kind: "loading" });
  const [runCount, setRunCount] = useState(0);

  useEffect(() => {
    let cancelled = false;
    setState({ kind: "loading" });
    fetchBenchmark()
      .then((data) => {
        if (!cancelled) setState({ kind: "ready", data });
      })
      .catch((err) => {
        if (!cancelled) {
          setState({ kind: "error", message: err instanceof ApiError ? err.message : "Unknown error" });
        }
      });
    return () => {
      cancelled = true;
    };
  }, [runCount]);

  if (state.kind === "loading") {
    return (
      <div className="p-8">
        <h1 className="text-2xl font-semibold text-white">Performance Lab</h1>
        <div className="mt-6 text-sm text-nexus-muted">
          Running a real multicore benchmark on this machine right now (no cached numbers)…
        </div>
      </div>
    );
  }
  if (state.kind === "error") {
    return (
      <div className="p-8">
        <h1 className="text-2xl font-semibold text-white">Performance Lab</h1>
        <div className="mt-6 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
          <div className="font-medium">Backend unreachable</div>
          <div className="mt-1 text-red-300/80">{state.message}</div>
        </div>
      </div>
    );
  }

  const { data } = state;
  const workers = data.entries.map((e) => e.num_workers);
  const medians = data.entries.map((e) => e.median_seconds);
  const speedups = data.entries.map((e) => e.speedup);
  const maxWorkers = Math.max(...workers);

  return (
    <div className="p-8 max-w-6xl">
      <div className="flex items-start justify-between">
        <div>
          <h1 className="text-2xl font-semibold text-white">Performance Lab</h1>
          <p className="text-nexus-muted mt-1 text-sm">
            Serial vs. multicore (std::thread, bounded worker pool) batch solve of {data.config.case_count}{" "}
            independent thermal cases (N={data.config.mesh_elements} mesh, seed={data.config.seed}), median of{" "}
            {data.config.repetitions} measured repetitions per worker count.
          </p>
        </div>
        <button
          onClick={() => setRunCount((c) => c + 1)}
          className="shrink-0 px-3 py-1.5 rounded text-sm border border-nexus-accent2 text-nexus-accent2 hover:bg-nexus-accent2/10"
        >
          Re-run benchmark
        </button>
      </div>

      {/* Hardware disclosure */}
      <div className="mt-6 rounded-lg border border-nexus-border bg-nexus-panel p-4">
        <div className="text-xs text-nexus-muted uppercase tracking-wide mb-2">
          Hardware &amp; build (disclosed, not assumed)
        </div>
        <div className="grid grid-cols-3 gap-y-1.5 gap-x-6 text-sm">
          <HwRow label="CPU" value={data.hardware.cpu_model} />
          <HwRow label="Logical cores" value={String(data.hardware.logical_cores)} />
          <HwRow
            label="Physical cores"
            value={`${data.hardware.physical_cores}${data.hardware.physical_cores_is_estimate ? " (estimate)" : ""}`}
          />
          <HwRow label="RAM" value={`${data.hardware.ram_mb} MB`} />
          <HwRow label="Compiler" value={data.hardware.compiler} />
          <HwRow label="Build type" value={data.hardware.build_type} />
          <HwRow label="Optimization flags" value={data.hardware.optimization_flags.trim() || "(none)"} />
        </div>
      </div>

      {maxWorkers < 4 && (
        <div className="mt-4 rounded-lg border border-amber-500/30 bg-amber-500/10 p-3 text-xs text-amber-300">
          This machine has only {data.hardware.logical_cores} logical CPU(s), so worker counts above that are
          never run (capped, not padded with fake entries) — only {workers.join(" and ")} worker(s) are shown
          below.
        </div>
      )}

      <div className="mt-6 grid grid-cols-3 gap-4">
        <SummaryCard
          label="Best speedup"
          value={`${Math.max(...speedups).toFixed(2)}×`}
        />
        <SummaryCard
          label="Best efficiency"
          value={`${(Math.max(...data.entries.map((e) => e.efficiency)) * 100).toFixed(0)}%`}
        />
        <SummaryCard
          label="Serial/parallel agreement"
          value={`${data.max_abs_agreement_error_C.toExponential(1)} °C`}
        />
      </div>

      <div className="mt-8 grid grid-cols-2 gap-4">
        <div>
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
            Median wall time by worker count
          </h2>
          <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
            <Plot
              data={[
                {
                  x: workers.map(String),
                  y: medians,
                  type: "bar",
                  marker: { color: "#38bdf8" },
                },
              ]}
              layout={{
                ...PLOT_LAYOUT_BASE,
                height: 300,
                xaxis: { ...PLOT_LAYOUT_BASE.xaxis, title: { text: "Workers" } },
                yaxis: { ...PLOT_LAYOUT_BASE.yaxis, title: { text: "Median time (s)" } },
              }}
              config={{ displayModeBar: false, responsive: true }}
              style={{ width: "100%" }}
            />
          </div>
        </div>

        <div>
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
            Speedup vs. ideal (linear) speedup
          </h2>
          <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
            <Plot
              data={[
                {
                  x: [0, maxWorkers],
                  y: [0, maxWorkers],
                  type: "scatter",
                  mode: "lines",
                  name: "Ideal (linear)",
                  line: { color: "#64748b", dash: "dot" },
                },
                {
                  x: workers,
                  y: speedups,
                  type: "scatter",
                  mode: "lines+markers",
                  name: "Measured",
                  line: { color: "#f472b6" },
                  marker: { size: 8 },
                },
              ]}
              layout={{
                ...PLOT_LAYOUT_BASE,
                height: 300,
                xaxis: { ...PLOT_LAYOUT_BASE.xaxis, title: { text: "Workers" } },
                yaxis: { ...PLOT_LAYOUT_BASE.yaxis, title: { text: "Speedup (T1/Tp)" } },
                legend: { orientation: "h" as const, y: -0.25 },
              }}
              config={{ displayModeBar: false, responsive: true }}
              style={{ width: "100%" }}
            />
          </div>
        </div>
      </div>

      <div className="mt-8">
        <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
          Raw results
        </h2>
        <div className="rounded-lg border border-nexus-border bg-nexus-panel overflow-hidden">
          <table className="w-full text-sm">
            <thead className="text-nexus-muted text-xs">
              <tr className="text-left border-b border-nexus-border">
                <th className="px-4 py-2">Workers</th>
                <th className="px-4 py-2">Median (s)</th>
                <th className="px-4 py-2">Speedup</th>
                <th className="px-4 py-2">Efficiency</th>
                <th className="px-4 py-2">All repetitions (s)</th>
              </tr>
            </thead>
            <tbody>
              {data.entries.map((e) => (
                <tr key={e.num_workers} className="border-b border-nexus-border/50">
                  <td className="px-4 py-2 text-white">{e.num_workers}</td>
                  <td className="px-4 py-2 text-nexus-muted">{e.median_seconds.toFixed(5)}</td>
                  <td className="px-4 py-2 text-nexus-muted">{e.num_workers === 1 ? "—" : `${e.speedup.toFixed(3)}×`}</td>
                  <td className="px-4 py-2 text-nexus-muted">
                    {e.num_workers === 1 ? "—" : `${(e.efficiency * 100).toFixed(1)}%`}
                  </td>
                  <td className="px-4 py-2 text-nexus-muted font-mono text-xs">
                    {e.rep_times_seconds.map((t) => t.toFixed(5)).join(", ")}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      <div className="mt-6 text-xs text-nexus-muted space-y-1">
        <p>
          <span className="text-nexus-muted/80">Warm-up:</span> {data.warmup_note}
        </p>
        <p>
          <span className="text-nexus-muted/80">Timing scope:</span> only the batch solve loop is timed;
          file loading, rendering and compilation are excluded. {data.launch_to_ready_note}
        </p>
        <p>
          Raw CSVs (per-repetition timings, summary, hardware) generated by{" "}
          <code>nexus_performance_export</code> and committed under <code>results/benchmarks/</code>.
        </p>
      </div>
    </div>
  );
}

function HwRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex gap-2 min-w-0">
      <span className="text-nexus-muted shrink-0">{label}:</span>
      <span className="text-white truncate">{value}</span>
    </div>
  );
}

function SummaryCard({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-lg border border-nexus-border bg-nexus-panel px-4 py-3">
      <div className="text-xs text-nexus-muted">{label}</div>
      <div className="text-lg text-white mt-1">{value}</div>
    </div>
  );
}
