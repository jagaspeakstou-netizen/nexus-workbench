interface StatusBadgeProps {
  implemented: boolean;
  tested: boolean;
}

// Deliberately has only two true states plus "pending" -- never a
// decorative "looks done" state. Mirrors the honesty constraint in
// backend/src/core/status.cpp 1:1.
export default function StatusBadge({ implemented, tested }: StatusBadgeProps) {
  if (implemented && tested) {
    return (
      <span className="inline-flex items-center gap-1.5 rounded-full bg-emerald-500/10 px-2.5 py-0.5 text-xs font-medium text-emerald-400">
        <span className="h-1.5 w-1.5 rounded-full bg-emerald-400" />
        Implemented &amp; tested
      </span>
    );
  }
  if (implemented) {
    return (
      <span className="inline-flex items-center gap-1.5 rounded-full bg-amber-500/10 px-2.5 py-0.5 text-xs font-medium text-amber-400">
        <span className="h-1.5 w-1.5 rounded-full bg-amber-400" />
        Implemented, untested
      </span>
    );
  }
  return (
    <span className="inline-flex items-center gap-1.5 rounded-full bg-white/5 px-2.5 py-0.5 text-xs font-medium text-nexus-muted">
      <span className="h-1.5 w-1.5 rounded-full bg-nexus-muted" />
      Pending
    </span>
  );
}
