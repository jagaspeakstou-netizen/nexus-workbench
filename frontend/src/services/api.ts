import type { BackendStatus } from "../types/status";
import type { ThermalStudy } from "../types/thermal";
import type { StlParseResponse, CsvParseResponse } from "../types/geometry";

// Single place that knows the backend's base URL. In dev, Vite proxies
// nothing -- we hit the C++ server directly on :8787 (CORS is enabled
// there specifically for this). In the packaged build, this should still
// resolve correctly since the backend always binds to 127.0.0.1.
const API_BASE = "http://127.0.0.1:8787";

export class ApiError extends Error {}

export async function fetchStatus(): Promise<BackendStatus> {
  let res: Response;
  try {
    res = await fetch(`${API_BASE}/api/status`);
  } catch (err) {
    throw new ApiError(
      "Could not reach the NEXUS backend. Is nexus_server running on port 8787?"
    );
  }
  if (!res.ok) {
    throw new ApiError(`Backend returned ${res.status} ${res.statusText}`);
  }
  return (await res.json()) as BackendStatus;
}

export async function fetchThermalStudy(): Promise<ThermalStudy> {
  let res: Response;
  try {
    res = await fetch(`${API_BASE}/api/thermal/study`);
  } catch (err) {
    throw new ApiError(
      "Could not reach the NEXUS backend. Is nexus_server running on port 8787?"
    );
  }
  if (!res.ok) {
    throw new ApiError(`Backend returned ${res.status} ${res.statusText}`);
  }
  return (await res.json()) as ThermalStudy;
}

async function postRawText<T>(path: string, body: string): Promise<T> {
  let res: Response;
  try {
    res = await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body,
    });
  } catch (err) {
    throw new ApiError("Could not reach the NEXUS backend. Is nexus_server running on port 8787?");
  }
  if (!res.ok) {
    throw new ApiError(`Backend returned ${res.status} ${res.statusText}`);
  }
  return (await res.json()) as T;
}

export function parseStl(content: string): Promise<StlParseResponse> {
  return postRawText<StlParseResponse>("/api/geometry/stl/parse", content);
}

export function parseCsv(content: string): Promise<CsvParseResponse> {
  return postRawText<CsvParseResponse>("/api/geometry/csv/parse", content);
}

async function fetchRawText(path: string): Promise<string> {
  let res: Response;
  try {
    res = await fetch(`${API_BASE}${path}`);
  } catch (err) {
    throw new ApiError("Could not reach the NEXUS backend. Is nexus_server running on port 8787?");
  }
  if (!res.ok) {
    throw new ApiError(`Backend returned ${res.status} ${res.statusText}`);
  }
  return res.text();
}

export function fetchSampleStl(): Promise<string> {
  return fetchRawText("/api/geometry/sample/stl");
}

export function fetchSampleCsv(): Promise<string> {
  return fetchRawText("/api/geometry/sample/csv");
}
