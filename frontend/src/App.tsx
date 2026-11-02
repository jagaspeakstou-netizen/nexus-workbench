import { useState } from "react";
import Sidebar from "./components/Sidebar";
import Overview from "./pages/Overview";
import ThermalAnalysis from "./pages/ThermalAnalysis";
import GeometryLab from "./pages/GeometryLab";
import PerformanceLab from "./pages/PerformanceLab";
import NeuralComputation from "./pages/NeuralComputation";
import HardwareIntegration from "./pages/HardwareIntegration";
import ModuleStatusPage from "./pages/ModuleStatusPage";

export default function App() {
  const [active, setActive] = useState("overview");

  return (
    <div className="flex min-h-screen bg-nexus-bg">
      <Sidebar activeId={active} onSelect={setActive} />
      <main className="flex-1">
        {active === "overview" ? (
          <Overview />
        ) : active === "thermal" ? (
          <ThermalAnalysis />
        ) : active === "geometry" ? (
          <GeometryLab />
        ) : active === "performance" ? (
          <PerformanceLab />
        ) : active === "neural" ? (
          <NeuralComputation />
        ) : active === "hardware" ? (
          <HardwareIntegration />
        ) : active === "validation" ? (
          <ModuleStatusPage
            moduleId="validation"
            docLinks={[{ label: "Full validation pass writeup", href: "docs/architecture/validation.md" }]}
          />
        ) : active === "docs" ? (
          <ModuleStatusPage
            moduleId="docs"
            docLinks={[
              { label: "Technical report", href: "docs/technical_report/REPORT.md" },
              { label: "AI-use log", href: "docs/technical_report/AI_USE_LOG.md" },
              { label: "Appendix", href: "docs/technical_report/APPENDIX.md" },
            ]}
          />
        ) : (
          // Fallback for any future sidebar id with no dedicated page yet --
          // still live-status-driven, not a hardcoded claim.
          <ModuleStatusPage moduleId={active} />
        )}
      </main>
    </div>
  );
}
