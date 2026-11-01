// Persistent sidebar, per brief section 2. Each entry's `id` matches a
// backend module id in /api/status (see types/status.ts) except
// "overview", which has no backend module of its own.
export interface SidebarItem {
  id: string;
  label: string;
}

export const SIDEBAR_ITEMS: SidebarItem[] = [
  { id: "overview", label: "Overview" },
  { id: "geometry", label: "Geometry Lab" },
  { id: "thermal", label: "Thermal Analysis" },
  { id: "performance", label: "Performance Lab" },
  { id: "hardware", label: "Hardware Integration" },
  { id: "neural", label: "Neural Computation" },
  { id: "validation", label: "Validation & Tests" },
  { id: "docs", label: "Documentation" },
];

interface SidebarProps {
  activeId: string;
  onSelect: (id: string) => void;
}

export default function Sidebar({ activeId, onSelect }: SidebarProps) {
  return (
    <nav className="w-60 shrink-0 border-r border-nexus-border bg-nexus-panel h-screen sticky top-0 flex flex-col">
      <div className="px-5 py-6 border-b border-nexus-border">
        <div className="text-lg font-semibold tracking-tight text-white">NEXUS</div>
        <div className="text-xs text-nexus-muted mt-1">Engineering Intelligence Workbench</div>
      </div>
      <ul className="flex-1 overflow-y-auto py-3">
        {SIDEBAR_ITEMS.map((item) => {
          const active = item.id === activeId;
          return (
            <li key={item.id}>
              <button
                onClick={() => onSelect(item.id)}
                className={[
                  "w-full text-left px-5 py-2.5 text-sm transition-colors border-l-2",
                  active
                    ? "border-nexus-accent bg-white/5 text-white"
                    : "border-transparent text-nexus-muted hover:text-nexus-text hover:bg-white/5",
                ].join(" ")}
              >
                {item.label}
              </button>
            </li>
          );
        })}
      </ul>
      <div className="px-5 py-4 border-t border-nexus-border text-[11px] text-nexus-muted">
        One workspace. Multiple engineering
        <br />
        disciplines. Verified computation.
      </div>
    </nav>
  );
}
