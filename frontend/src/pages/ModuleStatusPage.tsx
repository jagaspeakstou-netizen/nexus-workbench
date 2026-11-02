import { useEffect, useState } from "react";
import { fetchStatus, ApiError } from "../services/api";
import type { ModuleStatus } from "../types/status";
import StatusBadge from "../components/StatusBadge";

// Day 11/14 found this: App.tsx used to route "validation" and "docs" to
// a hardcoded PendingModule page unconditionally, regardless of what the
// live backend actually reported -- so once those two modules were
// implemented (Day 11 validation pass, Day 14 documentation), the
// sidebar still silently claimed "not implemented yet" for them, exactly
// the kind of stale claim this project's honesty rule exists to prevent.
// This page replaces that hardcoding: it fetches the same live
// /api/status the Overview page uses and renders whatever that module's
// entry actually says, so it can never drift out of sync with the
// backend again.
type LoadState =
  | { kind: "loading" }
  | { kind: "error"; message: string }
  | { kind: "ready"; module: ModuleStatus | null };

interface ModuleStatusPageProps {
  moduleId: string;
  docLinks?: { label: string; href: string }[];
}

export default function ModuleStatusPage({ moduleId, docLinks }: ModuleStatusPageProps) {
  const [state, setState] = useState<LoadState>({ kind: "loading" });

  useEffect(() => {
    let cancelled = false;
    fetchStatus()
      .then((data) => {
        if (cancelled) return;
        const mod = data.modules.find((m) => m.id === moduleId) ?? null;
        setState({ kind: "ready", module: mod });
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
  }, [moduleId]);

  return (
    <div className="p-8 max-w-3xl">
      {state.kind === "loading" && (
        <div className="text-sm text-nexus-muted">Connecting to backend…</div>
      )}

      {state.kind === "error" && (
        <div className="rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
          <div className="font-medium">Backend unreachable</div>
          <div className="mt-1 text-red-300/80">{state.message}</div>
          <div className="mt-2 text-red-300/60 text-xs">
            Run: <code>./backend/build/nexus_server</code> then reload this page.
          </div>
        </div>
      )}

      {state.kind === "ready" && state.module && (
        <>
          <div className="flex items-center justify-between">
            <h1 className="text-2xl font-semibold text-white">{state.module.label}</h1>
            <StatusBadge implemented={state.module.implemented} tested={state.module.tested} />
          </div>
          <p className="mt-4 text-sm text-nexus-text leading-relaxed">{state.module.note}</p>
          {docLinks && docLinks.length > 0 && (
            <div className="mt-6 rounded-lg border border-nexus-border bg-nexus-panel p-4">
              <div className="text-xs font-medium text-nexus-muted uppercase tracking-wide mb-2">
                Full writeup
              </div>
              <ul className="space-y-1">
                {docLinks.map((d) => (
                  <li key={d.href}>
                    <span className="text-sm text-nexus-accent">{d.label}</span>
                    <span className="text-xs text-nexus-muted ml-2">{d.href}</span>
                  </li>
                ))}
              </ul>
              <div className="text-xs text-nexus-muted/60 mt-2">
                Rendered from the repository docs, not fetched over HTTP — open these files directly.
              </div>
            </div>
          )}
        </>
      )}

      {state.kind === "ready" && !state.module && (
        <div className="text-sm text-nexus-muted">
          No status entry for "{moduleId}" was returned by the backend.
        </div>
      )}
    </div>
  );
}
