// Mirrors backend/src/api/main.cpp's hw_*_to_json() functions and the
// /api/hardware/report response body exactly.
//
// Everything here describes a MOCK / SIMULATED hardware session -- no
// physical microcontroller, FPGA, or JTAG probe exists. See
// backend/include/nexus/hardware_mocks.hpp and
// docs/architecture/hardware_mocks.md.

export interface HwDevice {
  id: string;
  type: "microcontroller" | "fpga" | "jtag";
  vendor: string;
  label: string;
  interface: string;
}

export interface HwConnectAttempt {
  attempt_number: number;
  outcome: "connected" | "timed_out" | "device_not_found";
  simulated_latency_ms: number;
}

export interface HwConnect {
  device_id: string;
  connected: boolean;
  attempts: HwConnectAttempt[];
  session_token: string;
}

export interface HwTelemetrySample {
  sample_index: number;
  timestamp_s: number;
  voltage_v: number;
  temperature_c: number;
  clock_locked: boolean;
  note: string;
}

export interface HwTelemetry {
  device_id: string;
  samples: HwTelemetrySample[];
  dropout_count: number;
  recovery_count: number;
  all_dropouts_recovered: boolean;
}

export interface HwDeviceEntry {
  device: HwDevice;
  connect: HwConnect;
  telemetry?: HwTelemetry;
}

export interface HwPin {
  name: string;
  function: string;
  voltage_v: number;
  direction: string;
}

export interface HwTypeGroup {
  type: "microcontroller" | "fpga" | "jtag";
  devices: HwDeviceEntry[];
  pinout: HwPin[];
}

export interface HwReport {
  mock_disclaimer: string;
  types: HwTypeGroup[];
}
