// Mirrors backend/src/api/main.cpp's neural_result_to_json() and the
// /api/neural/train response body exactly.

export interface NeuralParams {
  w: number;
  b: number;
  v: number;
  c: number;
}

export interface NeuralProblem {
  x: number;
  y_target: number;
  learning_rate: number;
  num_steps: number;
  init_params: NeuralParams;
}

export interface GradientCheckEntry {
  param_name: string;
  analytic: number;
  finite_difference: number;
  abs_difference: number;
  within_tolerance: boolean;
}

export interface GradientCheck {
  epsilon: number;
  tolerance: number;
  all_within_tolerance: boolean;
  entries: GradientCheckEntry[];
}

export interface TrainingStep {
  step: number;
  w: number;
  b: number;
  v: number;
  c: number;
  z: number;
  h: number;
  y_hat: number;
  loss: number;
  dw: number;
  db: number;
  dv: number;
  dc: number;
}

export interface NeuralTrainingResult {
  problem: NeuralProblem;
  gradient_check: GradientCheck;
  history: TrainingStep[];
  initial_loss: number;
  final_loss: number;
  final_params: NeuralParams;
}
