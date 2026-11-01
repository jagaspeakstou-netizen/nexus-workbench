import { useEffect, useState } from "react";
import { fetchStatus, ApiError } from "../services/api";
import type { BackendStatus } from "../types/status";
import StatusBadge from "../components/StatusBadge";

type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; data: BackendStatus };

export default function Overview() {
  const [state, setState] = useState<LoadState>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    fetchStatus()
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

  return (
    <div className="p-8 max-w-4xl">
      <h1 className="text-2xl font-semibold text-white">System Overview</h1>
      <p className="text-nexus-muted mt-1 text-sm">
        Live status, fetched from the C++ backend at <code>/api/status</code> — not hardcoded.
      </p>

      {state.kind === "loading" && (
        <div className="mt-8 text-sm text-nexus-muted">Connecting to backend…</div>
      )}

      {state.kind === "error" && (
        <div className="mt-8 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
          <div className="font-medium">Backend unreachable</div>
          <div className="mt-1 text-red-300/80">{state.message}</div>
          <div className="mt-2 text-red-300/60 text-xs">
            Run: <code>./backend/build/nexus_server</code> then reload this page.
          </div>
        </div>
      )}

      {state.kind === "ready" && (
        <>
          <div className="mt-6 grid grid-cols-3 gap-4">
            <InfoCard label="Application" value={state.data.app_name} />
            <InfoCard label="Version" value={state.data.version} />
            <InfoCard label="Backend standard" value={state.data.cpp_standard} />
          </div>

          <h2 className="mt-10 text-sm font-medium text-nexus-muted uppercase tracking-wide">
            Module status
          </h2>
          <div className="mt-3 divide-y divide-nexus-border rounded-lg border border-nexus-border bg-nexus-panel">
            {state.data.modules.map((m) => (
              <div key={m.id} className="flex items-center justify-between px-4 py-3">
                <div>
                  <div className="text-sm text-white">{m.label}</div>
                  <div className="text-xs text-nexus-muted mt-0.5">{m.note}</div>
                </div>
                <StatusBadge implemented={m.implemented} tested={m.tested} />
              </div>
            ))}
          </div>
        </>
      )}
    </div>
  );
}

function InfoCard({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-lg border border-nexus-border bg-nexus-panel px-4 py-3">
      <div className="text-xs text-nexus-muted">{label}</div>
      <div className="text-sm text-white mt-1 truncate">{value}</div>
    </div>
  );
}
