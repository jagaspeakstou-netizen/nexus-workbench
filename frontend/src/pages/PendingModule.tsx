// Honest empty state for modules not yet implemented (brief section 11:
// "Use only actual computed values or clearly labelled demonstration
// data" / section 4: "Every visible button must have a working action
// or be explicitly marked as unavailable"). Replaced page-by-page as
// each module's phase lands (see docs/architecture/schedule).
interface PendingModuleProps {
  label: string;
  dayPlanned: string;
}

export default function PendingModule({ label, dayPlanned }: PendingModuleProps) {
  return (
    <div className="p-8">
      <h1 className="text-2xl font-semibold text-white">{label}</h1>
      <div className="mt-6 rounded-lg border border-dashed border-nexus-border px-6 py-10 text-center">
        <div className="text-sm text-nexus-muted">
          This module is not implemented yet — scheduled for {dayPlanned}.
        </div>
        <div className="text-xs text-nexus-muted/60 mt-2">
          No mock data is shown here; the Overview page's status badges reflect this honestly.
        </div>
      </div>
    </div>
  );
}
