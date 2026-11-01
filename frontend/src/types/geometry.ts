// Mirrors backend/src/api/main.cpp's stl_result_to_json() / csv_result_to_json().

export type Vec3Tuple = [number, number, number];

export interface StlTriangle {
  normal: Vec3Tuple;
  v0: Vec3Tuple;
  v1: Vec3Tuple;
  v2: Vec3Tuple;
}

export interface StlParseResponse {
  valid: boolean;
  error?: string;
  solid_name?: string;
  triangle_count?: number;
  bounding_box?: { min: Vec3Tuple; max: Vec3Tuple };
  triangles?: StlTriangle[];
}

export interface AnsysNode {
  node_id: number;
  x_m: number;
  y_m: number;
  z_m: number;
  temperature_C: number;
}

export interface CsvParseResponse {
  valid: boolean;
  error?: string;
  node_count?: number;
  min_temperature_C?: number;
  max_temperature_C?: number;
  avg_temperature_C?: number;
  nodes?: AnsysNode[];
}
