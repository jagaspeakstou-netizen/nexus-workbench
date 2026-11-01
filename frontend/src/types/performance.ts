// Mirrors backend/src/api/main.cpp's performance_report_to_json() and
// the /api/performance/benchmark response body exactly.

export interface BenchmarkConfigDto {
  seed: number;
  case_count: number;
  mesh_elements: number;
  k_min: number;
  k_max: number;
  q_min: number;
  q_max: number;
  repetitions: number;
}

export interface HardwareInfoDto {
  cpu_model: string;
  logical_cores: number;
  physical_cores: number;
  physical_cores_is_estimate: boolean;
  ram_mb: number;
  compiler: string;
  build_type: string;
  optimization_flags: string;
}

export interface BenchmarkEntryDto {
  num_workers: number;
  rep_times_seconds: number[];
  median_seconds: number;
  speedup: number;
  efficiency: number;
}

export interface BenchmarkReportDto {
  config: BenchmarkConfigDto;
  hardware: HardwareInfoDto;
  entries: BenchmarkEntryDto[];
  max_abs_agreement_error_C: number;
  warmup_note: string;
  launch_to_ready_note: string;
}
