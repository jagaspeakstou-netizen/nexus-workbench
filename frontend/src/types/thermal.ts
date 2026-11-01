// Mirrors backend/src/api/main.cpp's thermal_result_to_json() and the
// /api/thermal/study response body exactly.

export interface ThermalProblem {
  L_m: number;
  A_m2: number;
  k_W_per_mK: number;
  q_W_per_m3: number;
  T0_C: number;
  TL_C: number;
}

export interface ThermalNode {
  index: number;
  x_m: number;
  temperature_C: number;
}

export interface ThermalMidpoint {
  element_index: number;
  x_m: number;
  fe_temperature_C: number;
  exact_temperature_C: number;
  abs_error_C: number;
}

export interface ThermalRun {
  num_elements: number;
  element_size_h_m: number;
  nodes: ThermalNode[];
  midpoints: ThermalMidpoint[];
  max_abs_midpoint_error_C: number;
  min_temperature_C: number;
  max_temperature_C: number;
}

export interface ThermalConvergence {
  num_elements: number[];
  element_size_h_m: number[];
  max_abs_midpoint_error_C: number[];
}

export interface ThermalStudy {
  problem: ThermalProblem;
  runs: ThermalRun[];
  convergence: ThermalConvergence;
}
