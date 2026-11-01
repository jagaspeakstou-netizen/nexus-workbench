import { useEffect, useMemo, useState } from "react";
import { fetchThermalStudy, ApiError } from "../services/api";
import type { ThermalStudy } from "../types/thermal";
import Plot from "../components/Plot";

type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; data: ThermalStudy };

// Dark-navy chart theme to match the rest of the UI (brief section 2).
const PLOT_LAYOUT_BASE = {
  paper_bgcolor: "#111726",
  plot_bgcolor: "#111726",
  font: { color: "#e2e8f0", size: 12 },
  margin: { l: 60, r: 20, t: 10, b: 50 },
  xaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
  yaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
  legend: { orientation: "h" as const, y: -0.2 },
};

export default function ThermalAnalysis() {
  const [state, setState] = useState<LoadState>({ kind: "loading" });
  const [selectedN, setSelectedN] = useState<number>(40);

  useEffect(() => {
    let cancelled = false;
    fetchThermalStudy()
      .then((data) => {
        if (!cancelled) setState({ kind: "ready", data });
      })
      .catch((err) => {
        if (!cancelled) {
          const message = err instanceof ApiError ? err.message : "Unknown error";
          setState({ kind: "error", message });
        }
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const selectedRun = useMemo(() => {
    if (state.kind !== "ready") return undefined;
    return state.data.runs.find((r) => r.num_elements === selectedN);
  }, [state, selectedN]);

  function downloadCsv() {
    if (state.kind !== "ready" || !selectedRun) return;
    const header = "kind,index,x_m,temperature_fe_C,temperature_exact_C,abs_error_C\n";
    const nodeRows = selectedRun.nodes
      .map((n) => `node,${n.index},${n.x_m},${n.temperature_C},,`)
      .join("\n");
    const midRows = selectedRun.midpoints
      .map(
        (m) =>
          `midpoint,${m.element_index},${m.x_m},${m.fe_temperature_C},${m.exact_temperature_C},${m.abs_error_C}`
      )
      .join("\n");
    const csv = header + nodeRows + "\n" + midRows + "\n";
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `thermal_n${selectedRun.num_elements}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }

  if (state.kind === "loading") {
    return <div className="p-8 text-sm text-nexus-muted">Connecting to backend…</div>;
  }
  if (state.kind === "error") {
    return (
      <div className="p-8">
        <h1 className="text-2xl font-semibold text-white">Thermal Analysis</h1>
        <div className="mt-6 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
          <div className="font-medium">Backend unreachable</div>
          <div className="mt-1 text-red-300/80">{state.message}</div>
        </div>
      </div>
    );
  }

  const { data } = state;

  return (
    <div className="p-8 max-w-6xl">
      <h1 className="text-2xl font-semibold text-white">Thermal Analysis</h1>
      <p className="text-nexus-muted mt-1 text-sm">
        1D steady-state FEA, two-node linear elements · L={data.problem.L_m} m, k=
        {data.problem.k_W_per_mK} W/(m·K), q={data.problem.q_W_per_m3} W/m³, T(0)=
        {data.problem.T0_C}°C, T(L)={data.problem.TL_C}°C
      </p>

      <div className="mt-6 flex items-center gap-3">
        <label className="text-sm text-nexus-muted">Mesh (N elements):</label>
        <div className="flex gap-1">
          {data.convergence.num_elements.map((n) => (
            <button
              key={n}
              onClick={() => setSelectedN(n)}
              className={[
                "px-3 py-1.5 rounded text-sm border",
                n === selectedN
                  ? "border-nexus-accent bg-nexus-accent/10 text-nexus-accent"
                  : "border-nexus-border text-nexus-muted hover:text-nexus-text",
              ].join(" ")}
            >
              N={n}
            </button>
          ))}
        </div>
        <button
          onClick={downloadCsv}
          className="ml-auto px-3 py-1.5 rounded text-sm border border-nexus-border text-nexus-muted hover:text-nexus-text hover:border-nexus-accent2"
        >
          Export CSV (N={selectedN})
        </button>
      </div>

      {selectedRun && (
        <div className="mt-6 grid grid-cols-4 gap-4">
          <SummaryCard label="Mesh size" value={`N=${selectedRun.num_elements}`} />
          <SummaryCard label="Max temperature" value={`${selectedRun.max_temperature_C.toFixed(2)} °C`} />
          <SummaryCard label="Min temperature" value={`${selectedRun.min_temperature_C.toFixed(2)} °C`} />
          <SummaryCard
            label="Max midpoint error"
            value={`${selectedRun.max_abs_midpoint_error_C.toFixed(4)} °C`}
          />
        </div>
      )}

      {selectedRun && (
        <div className="mt-8">
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
            Temperature vs. position (N={selectedRun.num_elements})
          </h2>
          <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
            <Plot
              data={[
                {
                  x: selectedRun.nodes.map((n) => n.x_m),
                  y: selectedRun.nodes.map((n) => n.temperature_C),
                  type: "scatter",
                  mode: "lines+markers",
                  name: "FE nodal temperature",
                  line: { color: "#38bdf8" },
                  marker: { size: 5 },
                },
                {
                  x: selectedRun.midpoints.map((m) => m.x_m),
                  y: selectedRun.midpoints.map((m) => m.exact_temperature_C),
                  type: "scatter",
                  mode: "lines",
                  name: "Analytical T_exact(x)",
                  line: { color: "#818cf8", dash: "dot" },
                },
              ]}
              layout={{
                ...PLOT_LAYOUT_BASE,
                height: 340,
                xaxis: { ...PLOT_LAYOUT_BASE.xaxis, title: { text: "Position x (m)" } },
                yaxis: { ...PLOT_LAYOUT_BASE.yaxis, title: { text: "Temperature (°C)" } },
              }}
              config={{ displayModeBar: false, responsive: true }}
              style={{ width: "100%" }}
            />
          </div>
        </div>
      )}

      <div className="mt-8">
        <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
          Mesh convergence — max |FE_midpoint − exact| vs. element size h
        </h2>
        <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
          <Plot
            data={[
              {
                x: data.convergence.element_size_h_m,
                y: data.convergence.max_abs_midpoint_error_C,
                type: "scatter",
                mode: "lines+markers",
                name: "Max midpoint error",
                line: { color: "#38bdf8" },
                marker: { size: 7 },
              },
            ]}
            layout={{
              ...PLOT_LAYOUT_BASE,
              height: 320,
              xaxis: {
                ...PLOT_LAYOUT_BASE.xaxis,
                title: { text: "Element size h (m)" },
                type: "log",
                dtick: 0.30103, // one tick per factor of 2 (log10(2)) — matches N=10/20/40/80
              },
              yaxis: {
                ...PLOT_LAYOUT_BASE.yaxis,
                title: { text: "Max abs. midpoint error (°C)" },
                type: "log",
                dtick: 1, // one tick per power of 10
              },
            }}
            config={{ displayModeBar: false, responsive: true }}
            style={{ width: "100%" }}
          />
        </div>
        <p className="text-xs text-nexus-muted mt-2">
          Error falls ~4× each time N doubles — consistent with O(h²) convergence for linear
          elements. See Documentation for the closed-form derivation this was checked against.
        </p>
      </div>
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
