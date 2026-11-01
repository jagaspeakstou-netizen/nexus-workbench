import { useEffect, useState } from "react";
import { fetchNeuralTraining, ApiError } from "../services/api";
import type { NeuralTrainingResult } from "../types/neural";
import Plot from "../components/Plot";

type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; data: NeuralTrainingResult };

const PLOT_LAYOUT_BASE = {
  paper_bgcolor: "#111726",
  plot_bgcolor: "#111726",
  font: { color: "#e2e8f0", size: 12 },
  margin: { l: 55, r: 20, t: 10, b: 45 },
  xaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
  yaxis: { gridcolor: "#1f2937", zerolinecolor: "#1f2937" },
};

export default function NeuralComputation() {
  const [state, setState] = useState<LoadState>({ kind: "loading" });
  const [runCount, setRunCount] = useState(0);

  useEffect(() => {
    let cancelled = false;
    setState({ kind: "loading" });
    fetchNeuralTraining()
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
        <h1 className="text-2xl font-semibold text-white">Neural Computation</h1>
        <div className="mt-6 text-sm text-nexus-muted">Running backprop training on the backend…</div>
      </div>
    );
  }
  if (state.kind === "error") {
    return (
      <div className="p-8">
        <h1 className="text-2xl font-semibold text-white">Neural Computation</h1>
        <div className="mt-6 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
          <div className="font-medium">Backend unreachable</div>
          <div className="mt-1 text-red-300/80">{state.message}</div>
        </div>
      </div>
    );
  }

  const { data } = state;
  const steps = data.history.map((h) => h.step);
  const losses = data.history.map((h) => h.loss);
  const yHats = data.history.map((h) => h.y_hat);

  return (
    <div className="p-8 max-w-6xl">
      <div className="flex items-start justify-between">
        <div>
          <h1 className="text-2xl font-semibold text-white">Neural Computation</h1>
          <p className="text-nexus-muted mt-1 text-sm">
            Hand-implemented forward + backward pass (no autodiff) for a single tanh hidden unit,
            trained {data.problem.num_steps} steps by plain gradient descent (lr=
            {data.problem.learning_rate}) on one example (x={data.problem.x}, target y=
            {data.problem.y_target}).
          </p>
        </div>
        <button
          onClick={() => setRunCount((c) => c + 1)}
          className="shrink-0 px-3 py-1.5 rounded text-sm border border-nexus-accent2 text-nexus-accent2 hover:bg-nexus-accent2/10"
        >
          Re-run training
        </button>
      </div>

      {/* Computational graph */}
      <div className="mt-6 rounded-lg border border-nexus-border bg-nexus-panel p-4">
        <div className="text-xs text-nexus-muted uppercase tracking-wide mb-3">Computational graph</div>
        <ComputationalGraph />
      </div>

      {/* Gradient check */}
      <div className="mt-6 rounded-lg border border-nexus-border bg-nexus-panel p-4">
        <div className="flex items-center justify-between mb-2">
          <div className="text-xs text-nexus-muted uppercase tracking-wide">
            Gradient check (analytic backward() vs. central finite difference)
          </div>
          <span
            className={`text-xs px-2 py-0.5 rounded ${
              data.gradient_check.all_within_tolerance
                ? "bg-emerald-500/15 text-emerald-300"
                : "bg-red-500/15 text-red-300"
            }`}
          >
            {data.gradient_check.all_within_tolerance ? "ALL WITHIN TOLERANCE" : "FAILED"}
          </span>
        </div>
        <div className="text-xs text-nexus-muted mb-3">
          ε={data.gradient_check.epsilon.toExponential(0)}, tolerance=
          {data.gradient_check.tolerance.toExponential(0)} — evaluated once, at the initial
          parameters, before any training step has moved them.
        </div>
        <table className="w-full text-sm">
          <thead className="text-nexus-muted text-xs">
            <tr className="text-left border-b border-nexus-border">
              <th className="px-3 py-1.5">Parameter</th>
              <th className="px-3 py-1.5">Analytic dL/dp</th>
              <th className="px-3 py-1.5">Finite difference</th>
              <th className="px-3 py-1.5">|difference|</th>
              <th className="px-3 py-1.5">Within tolerance</th>
            </tr>
          </thead>
          <tbody>
            {data.gradient_check.entries.map((e) => (
              <tr key={e.param_name} className="border-b border-nexus-border/50">
                <td className="px-3 py-1.5 text-white font-mono">{e.param_name}</td>
                <td className="px-3 py-1.5 text-nexus-muted font-mono">{e.analytic.toFixed(10)}</td>
                <td className="px-3 py-1.5 text-nexus-muted font-mono">{e.finite_difference.toFixed(10)}</td>
                <td className="px-3 py-1.5 text-nexus-muted font-mono">{e.abs_difference.toExponential(3)}</td>
                <td className="px-3 py-1.5">
                  {e.within_tolerance ? (
                    <span className="text-emerald-300">OK</span>
                  ) : (
                    <span className="text-red-300">FAIL</span>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Training curves */}
      <div className="mt-6 grid grid-cols-2 gap-4">
        <div>
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
            Loss vs. training step (log scale)
          </h2>
          <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
            <Plot
              data={[
                {
                  x: steps,
                  y: losses,
                  type: "scatter",
                  mode: "lines",
                  line: { color: "#38bdf8" },
                },
              ]}
              layout={{
                ...PLOT_LAYOUT_BASE,
                height: 280,
                xaxis: { ...PLOT_LAYOUT_BASE.xaxis, title: { text: "Step" } },
                yaxis: { ...PLOT_LAYOUT_BASE.yaxis, title: { text: "Loss" }, type: "log" },
              }}
              config={{ displayModeBar: false, responsive: true }}
              style={{ width: "100%" }}
            />
          </div>
        </div>
        <div>
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide mb-2">
            Prediction (y_hat) converging to target
          </h2>
          <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
            <Plot
              data={[
                {
                  x: [0, data.problem.num_steps],
                  y: [data.problem.y_target, data.problem.y_target],
                  type: "scatter",
                  mode: "lines",
                  name: "Target",
                  line: { color: "#64748b", dash: "dot" },
                },
                {
                  x: steps,
                  y: yHats,
                  type: "scatter",
                  mode: "lines",
                  name: "y_hat",
                  line: { color: "#f472b6" },
                },
              ]}
              layout={{
                ...PLOT_LAYOUT_BASE,
                height: 280,
                xaxis: { ...PLOT_LAYOUT_BASE.xaxis, title: { text: "Step" } },
                yaxis: { ...PLOT_LAYOUT_BASE.yaxis, title: { text: "y_hat" } },
                legend: { orientation: "h" as const, y: -0.3 },
              }}
              config={{ displayModeBar: false, responsive: true }}
              style={{ width: "100%" }}
            />
          </div>
        </div>
      </div>

      <div className="mt-6 grid grid-cols-3 gap-4">
        <SummaryCard label="Initial loss" value={data.initial_loss.toFixed(4)} />
        <SummaryCard label="Final loss" value={data.final_loss.toExponential(3)} />
        <SummaryCard label="Final y_hat" value={data.final_params ? yHats[yHats.length - 1]?.toFixed(6) ?? "—" : "—"} />
      </div>

      <div className="mt-6 text-xs text-nexus-muted space-y-1">
        <p>
          Forward and backward passes are hand-derived (no autodiff library) — see{" "}
          <code>backend/include/nexus/neural.hpp</code>, <code>backend/src/neural/neural.cpp</code>,
          and the full chain-rule derivation in <code>docs/architecture/backprop.md</code>.
        </p>
        <p>
          Raw CSVs (per-step training trace, gradient check) generated by{" "}
          <code>nexus_neural_export</code> and committed under <code>results/csv/</code>.
        </p>
      </div>
    </div>
  );
}

function SummaryCard({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-lg border border-nexus-border bg-nexus-panel px-4 py-3">
      <div className="text-xs text-nexus-muted">{label}</div>
      <div className="text-lg text-white mt-1 font-mono">{value}</div>
    </div>
  );
}

// Plain inline SVG, not a diagram library -- five nodes and four labelled
// edges is simple enough that a library would be more code than the
// diagram itself.
function ComputationalGraph() {
  return (
    <svg viewBox="0 0 760 140" className="w-full h-auto" role="img" aria-label="Computational graph: x through w,b to tanh to h, through v,c to y_hat">
      <defs>
        <marker id="arrow" markerWidth="8" markerHeight="8" refX="6" refY="4" orient="auto">
          <path d="M0,0 L8,4 L0,8 Z" fill="#64748b" />
        </marker>
      </defs>
      {/* edges */}
      <line x1="60" y1="70" x2="190" y2="70" stroke="#64748b" strokeWidth="2" markerEnd="url(#arrow)" />
      <line x1="250" y1="70" x2="380" y2="70" stroke="#64748b" strokeWidth="2" markerEnd="url(#arrow)" />
      <line x1="440" y1="70" x2="570" y2="70" stroke="#64748b" strokeWidth="2" markerEnd="url(#arrow)" />
      <line x1="630" y1="70" x2="700" y2="70" stroke="#64748b" strokeWidth="2" markerEnd="url(#arrow)" />

      <text x="125" y="58" textAnchor="middle" fontSize="12" fill="#94a3b8">w, b</text>
      <text x="315" y="58" textAnchor="middle" fontSize="12" fill="#94a3b8">tanh</text>
      <text x="505" y="58" textAnchor="middle" fontSize="12" fill="#94a3b8">v, c</text>

      {/* nodes */}
      <circle cx="35" cy="70" r="25" fill="#111726" stroke="#38bdf8" strokeWidth="2" />
      <text x="35" y="75" textAnchor="middle" fontSize="14" fill="#e2e8f0">x</text>

      <circle cx="220" cy="70" r="25" fill="#111726" stroke="#f472b6" strokeWidth="2" />
      <text x="220" y="75" textAnchor="middle" fontSize="13" fill="#e2e8f0">z</text>

      <circle cx="410" cy="70" r="25" fill="#111726" stroke="#f472b6" strokeWidth="2" />
      <text x="410" y="75" textAnchor="middle" fontSize="13" fill="#e2e8f0">h</text>

      <circle cx="600" cy="70" r="25" fill="#111726" stroke="#34d399" strokeWidth="2" />
      <text x="600" y="75" textAnchor="middle" fontSize="12" fill="#e2e8f0">ŷ</text>

      <circle cx="725" cy="70" r="25" fill="#111726" stroke="#f59e0b" strokeWidth="2" />
      <text x="725" y="75" textAnchor="middle" fontSize="11" fill="#e2e8f0">L</text>
    </svg>
  );
}
