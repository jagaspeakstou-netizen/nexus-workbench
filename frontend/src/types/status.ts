// Mirrors backend/include/nexus/status.hpp's JSON shape exactly (see
// module_status_to_json in backend/src/api/main.cpp). Keeping this in
// one place is the "API contract" Task A asks for (section 4 of the
// brief) -- every component that reads /api/status imports this type
// instead of re-declaring its own shape.

export interface ModuleStatus {
  id: string;
  label: string;
  implemented: boolean;
  tested: boolean;
  note: string;
}

export interface BackendStatus {
  app_name: string;
  version: string;
  cpp_standard: string;
  modules: ModuleStatus[];
}
