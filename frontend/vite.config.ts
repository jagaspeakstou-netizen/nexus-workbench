import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// NEXUS frontend dev server. In development, the Vite dev server runs on
// :5173 and the C++ backend (nexus_server) runs separately on :8787; the
// frontend talks to it over plain HTTP via services/api.ts. In the packaged
// Windows build, the WebView2 shell points at the built static files and
// the backend serves the same /api/* routes on localhost (see Day 12).
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
  },
});
