import { useState } from "react";
import Sidebar from "./components/Sidebar";
import Overview from "./pages/Overview";
import ThermalAnalysis from "./pages/ThermalAnalysis";
import GeometryLab from "./pages/GeometryLab";
import PerformanceLab from "./pages/PerformanceLab";
import NeuralComputation from "./pages/NeuralComputation";
import PendingModule from "./pages/PendingModule";

const PENDING_DAYS: Record<string, string> = {
  hardware: "Day 10",
  validation: "Day 11",
  docs: "Day 14",
};

const LABELS: Record<string, string> = {
  geometry: "Geometry Lab",
  thermal: "Thermal Analysis",
  performance: "Performance Lab",
  hardware: "Hardware Integration",
  neural: "Neural Computation",
  validation: "Validation & Tests",
  docs: "Documentation",
};

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
        ) : (
          <PendingModule label={LABELS[active]} dayPlanned={PENDING_DAYS[active]} />
        )}
      </main>
    </div>
  );
}
