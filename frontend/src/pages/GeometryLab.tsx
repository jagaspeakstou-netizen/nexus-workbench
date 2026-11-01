import { useRef, useState } from "react";
import { parseStl, parseCsv, fetchSampleStl, fetchSampleCsv, ApiError } from "../services/api";
import type { StlParseResponse, CsvParseResponse, AnsysNode } from "../types/geometry";
import MeshViewer from "../components/MeshViewer";
import PointCloudViewer from "../components/PointCloudViewer";
import TemperatureLegend from "../components/TemperatureLegend";

type Panel<T> = { status: "idle" } | { status: "loading" } | { status: "error"; message: string } | { status: "ready"; data: T };

export default function GeometryLab() {
  const [stlPanel, setStlPanel] = useState<Panel<StlParseResponse>>({ status: "idle" });
  const [csvPanel, setCsvPanel] = useState<Panel<CsvParseResponse>>({ status: "idle" });
  const [selectedNode, setSelectedNode] = useState<AnsysNode | null>(null);

  const stlInputRef = useRef<HTMLInputElement>(null);
  const csvInputRef = useRef<HTMLInputElement>(null);

  async function handleStlText(text: string) {
    setStlPanel({ status: "loading" });
    try {
      const data = await parseStl(text);
      if (!data.valid) {
        setStlPanel({ status: "error", message: data.error ?? "Invalid STL file." });
        return;
      }
      setStlPanel({ status: "ready", data });
    } catch (err) {
      setStlPanel({ status: "error", message: err instanceof ApiError ? err.message : "Unknown error" });
    }
  }

  async function handleCsvText(text: string) {
    setCsvPanel({ status: "loading" });
    setSelectedNode(null);
    try {
      const data = await parseCsv(text);
      if (!data.valid) {
        setCsvPanel({ status: "error", message: data.error ?? "Invalid CSV file." });
        return;
      }
      setCsvPanel({ status: "ready", data });
    } catch (err) {
      setCsvPanel({ status: "error", message: err instanceof ApiError ? err.message : "Unknown error" });
    }
  }

  function onStlFileChosen(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    file.text().then(handleStlText);
  }

  function onCsvFileChosen(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    file.text().then(handleCsvText);
  }

  async function loadSampleStl() {
    setStlPanel({ status: "loading" });
    try {
      const text = await fetchSampleStl();
      await handleStlText(text);
    } catch (err) {
      setStlPanel({ status: "error", message: err instanceof ApiError ? err.message : "Unknown error" });
    }
  }

  async function loadSampleCsv() {
    setCsvPanel({ status: "loading" });
    try {
      const text = await fetchSampleCsv();
      await handleCsvText(text);
    } catch (err) {
      setCsvPanel({ status: "error", message: err instanceof ApiError ? err.message : "Unknown error" });
    }
  }

  return (
    <div className="p-8 max-w-6xl space-y-12">
      <div>
        <h1 className="text-2xl font-semibold text-white">Geometry Lab</h1>
        <p className="text-nexus-muted mt-1 text-sm">
          ASCII STL and ANSYS nodal-temperature CSV import, parsed and validated in C++.
        </p>
      </div>

      {/* --- ASCII STL --- */}
      <section>
        <div className="flex items-center justify-between">
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide">CAD geometry (ASCII STL)</h2>
          <div className="flex gap-2">
            <button
              onClick={loadSampleStl}
              className="px-3 py-1.5 rounded text-sm border border-nexus-border text-nexus-muted hover:text-white hover:border-nexus-accent"
            >
              Load sample (self-authored tetrahedron)
            </button>
            <button
              onClick={() => stlInputRef.current?.click()}
              className="px-3 py-1.5 rounded text-sm border border-nexus-accent2 text-nexus-accent2 hover:bg-nexus-accent2/10"
            >
              Choose .stl file…
            </button>
            <input ref={stlInputRef} type="file" accept=".stl" className="hidden" onChange={onStlFileChosen} />
          </div>
        </div>

        {stlPanel.status === "idle" && (
          <div className="mt-4 rounded-lg border border-dashed border-nexus-border px-6 py-10 text-center text-sm text-nexus-muted">
            No STL loaded yet.
          </div>
        )}
        {stlPanel.status === "loading" && <div className="mt-4 text-sm text-nexus-muted">Parsing…</div>}
        {stlPanel.status === "error" && (
          <div className="mt-4 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
            <div className="font-medium">Rejected by the C++ parser</div>
            <div className="mt-1 text-red-300/80 font-mono text-xs">{stlPanel.message}</div>
          </div>
        )}
        {stlPanel.status === "ready" && stlPanel.data.triangles && stlPanel.data.bounding_box && (
          <>
            <div className="mt-4 grid grid-cols-4 gap-4">
              <InfoCard label="Solid name" value={stlPanel.data.solid_name || "(unnamed)"} />
              <InfoCard label="Triangle count" value={String(stlPanel.data.triangle_count)} />
              <InfoCard
                label="Bounding box min"
                value={stlPanel.data.bounding_box.min.map((v) => v.toFixed(3)).join(", ")}
              />
              <InfoCard
                label="Bounding box max"
                value={stlPanel.data.bounding_box.max.map((v) => v.toFixed(3)).join(", ")}
              />
            </div>
            <div className="mt-4 rounded-lg border border-nexus-border bg-nexus-panel p-2">
              <MeshViewer triangles={stlPanel.data.triangles} boundingBox={stlPanel.data.bounding_box} />
            </div>
            <p className="text-xs text-nexus-muted mt-2">Drag to rotate · scroll to zoom · right-drag to pan.</p>
          </>
        )}
      </section>

      {/* --- ANSYS CSV --- */}
      <section>
        <div className="flex items-center justify-between">
          <h2 className="text-sm font-medium text-nexus-muted uppercase tracking-wide">
            ANSYS nodal temperatures (CSV) — synthetic sample, not real solver output
          </h2>
          <div className="flex gap-2">
            <button
              onClick={loadSampleCsv}
              className="px-3 py-1.5 rounded text-sm border border-nexus-border text-nexus-muted hover:text-white hover:border-nexus-accent"
            >
              Load sample (synthetic)
            </button>
            <button
              onClick={() => csvInputRef.current?.click()}
              className="px-3 py-1.5 rounded text-sm border border-nexus-accent2 text-nexus-accent2 hover:bg-nexus-accent2/10"
            >
              Choose .csv file…
            </button>
            <input ref={csvInputRef} type="file" accept=".csv" className="hidden" onChange={onCsvFileChosen} />
          </div>
        </div>

        {csvPanel.status === "idle" && (
          <div className="mt-4 rounded-lg border border-dashed border-nexus-border px-6 py-10 text-center text-sm text-nexus-muted">
            No CSV loaded yet. Expected schema: <code>node_id,x_m,y_m,z_m,temperature_C</code>
          </div>
        )}
        {csvPanel.status === "loading" && <div className="mt-4 text-sm text-nexus-muted">Parsing…</div>}
        {csvPanel.status === "error" && (
          <div className="mt-4 rounded-lg border border-red-500/30 bg-red-500/10 p-4 text-sm text-red-300">
            <div className="font-medium">Rejected by the C++ parser</div>
            <div className="mt-1 text-red-300/80 font-mono text-xs">{csvPanel.message}</div>
          </div>
        )}
        {csvPanel.status === "ready" && csvPanel.data.nodes && (
          <>
            <div className="mt-4 grid grid-cols-4 gap-4">
              <InfoCard label="Node count" value={String(csvPanel.data.node_count)} />
              <InfoCard label="Min temperature" value={`${csvPanel.data.min_temperature_C?.toFixed(2)} °C`} />
              <InfoCard label="Max temperature" value={`${csvPanel.data.max_temperature_C?.toFixed(2)} °C`} />
              <InfoCard label="Avg temperature" value={`${csvPanel.data.avg_temperature_C?.toFixed(2)} °C`} />
            </div>

            <div className="mt-4 flex items-center justify-between">
              <TemperatureLegend min={csvPanel.data.min_temperature_C!} max={csvPanel.data.max_temperature_C!} />
            </div>

            <div className="mt-2 grid grid-cols-2 gap-4">
              <div className="rounded-lg border border-nexus-border bg-nexus-panel p-2">
                <PointCloudViewer
                  nodes={csvPanel.data.nodes}
                  minTemp={csvPanel.data.min_temperature_C!}
                  maxTemp={csvPanel.data.max_temperature_C!}
                  onSelect={setSelectedNode}
                />
                <p className="text-xs text-nexus-muted mt-2 px-1">Click a point to inspect it below.</p>
              </div>

              <div className="rounded-lg border border-nexus-border bg-nexus-panel overflow-y-auto max-h-[420px]">
                <table className="w-full text-xs">
                  <thead className="sticky top-0 bg-nexus-panel text-nexus-muted">
                    <tr className="text-left border-b border-nexus-border">
                      <th className="px-3 py-2">Node</th>
                      <th className="px-3 py-2">x (m)</th>
                      <th className="px-3 py-2">y (m)</th>
                      <th className="px-3 py-2">z (m)</th>
                      <th className="px-3 py-2">T (°C)</th>
                    </tr>
                  </thead>
                  <tbody>
                    {csvPanel.data.nodes.map((n) => (
                      <tr
                        key={n.node_id}
                        onClick={() => setSelectedNode(n)}
                        className={[
                          "cursor-pointer border-b border-nexus-border/50",
                          selectedNode?.node_id === n.node_id ? "bg-nexus-accent/10" : "hover:bg-white/5",
                        ].join(" ")}
                      >
                        <td className="px-3 py-1.5 text-white">#{n.node_id}</td>
                        <td className="px-3 py-1.5 text-nexus-muted">{n.x_m.toFixed(3)}</td>
                        <td className="px-3 py-1.5 text-nexus-muted">{n.y_m.toFixed(3)}</td>
                        <td className="px-3 py-1.5 text-nexus-muted">{n.z_m.toFixed(3)}</td>
                        <td className="px-3 py-1.5 text-white">{n.temperature_C.toFixed(2)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </>
        )}
      </section>
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
