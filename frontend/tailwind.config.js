/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        // Dark navy engineering palette (brief section 2: design direction).
        nexus: {
          bg: "#0a0e1a",
          panel: "#111726",
          border: "#1f2937",
          accent: "#38bdf8",   // cyan
          accent2: "#818cf8",  // violet
          text: "#e2e8f0",
          muted: "#64748b",
        },
      },
    },
  },
  plugins: [],
};
