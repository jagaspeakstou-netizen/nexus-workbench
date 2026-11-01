interface TemperatureLegendProps {
  min: number;
  max: number;
}

export default function TemperatureLegend({ min, max }: TemperatureLegendProps) {
  const mid = (min + max) / 2;
  return (
    <div className="flex items-center gap-3 text-xs text-nexus-muted">
      <span>{min.toFixed(1)} °C</span>
      <div
        className="h-2 w-40 rounded-full"
        style={{ background: "linear-gradient(to right, #38bdf8, #f472b6)" }}
      />
      <span>{max.toFixed(1)} °C</span>
      <span className="text-nexus-muted/60">(mid {mid.toFixed(1)} °C)</span>
    </div>
  );
}
