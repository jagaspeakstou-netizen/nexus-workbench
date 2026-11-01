// Module E -- Hardware Integration (MOCK adapters).
//
// IMPORTANT, READ FIRST: nothing in this file or its implementation talks
// to real hardware. There is no microcontroller, FPGA, or JTAG probe
// attached to this project -- everything here is a deterministic software
// simulator standing in for one, built because the assignment calls for
// adapter *interfaces* (discovery/connect/telemetry/timeout-recovery)
// without requiring physical hardware on hand. Every public name in this
// header says "mock" or "simulated" somewhere nearby on purpose, and the
// frontend page and docs for this module repeat the same labeling, so
// nothing here is ever mistaken for a real device readout.
//
// What IS real: the adapter *logic* -- discovery returning a fixed
// catalog, a connect handshake with scripted retry/backoff, telemetry
// sampling with a scripted mid-session dropout and automatic recovery.
// All of it is deterministic (seeded by device id, not wall-clock or
// std::random_device) so the same call always produces the same
// sequence, and is exercised by real unit tests in test_hardware.cpp --
// it is a genuine, testable implementation of the adapter pattern, just
// one whose "device" is simulated rather than physical.

#pragma once

#include <string>
#include <vector>

namespace nexus::hardware {

enum class DeviceType { Microcontroller, Fpga, Jtag };

std::string to_string(DeviceType type);

struct DeviceDescriptor {
    std::string id;         // e.g. "mcu-sim-01"
    DeviceType type;
    std::string vendor;     // clearly fictional/generic, e.g. "Mock Semiconductor Co."
    std::string label;      // e.g. "8-bit AVR-class MCU (simulated)"
    // Named connection_interface, not `interface` -- on Windows,
    // <combaseapi.h> (pulled in transitively by the Windows SDK headers)
    // #defines `interface` to `struct` for COM programming, which silently
    // breaks a member named `interface` under MSVC specifically (it parses
    // fine with GCC/Clang, which is exactly what made this a real,
    // disclosed Windows-only build failure the first time this module was
    // built on real Windows CI -- see docs/architecture/hardware_mocks.md).
    std::string connection_interface;  // e.g. "UART (simulated, mock-COM3)"
};

// The fixed catalog of mock devices this adapter type exposes. Pure
// function of `type` -- same input, same output, every call. (A real
// adapter's discover() would poll a bus; a simulator has nothing to poll,
// so returning a fixed catalog is the honest mock equivalent, not a
// shortcut hiding real nondeterminism.)
std::vector<DeviceDescriptor> discover_devices(DeviceType type);

enum class ConnectOutcome { Connected, TimedOut, DeviceNotFound };

struct ConnectAttempt {
    int attempt_number = 1;  // 1-based
    ConnectOutcome outcome = ConnectOutcome::TimedOut;
    double simulated_latency_ms = 0.0;
};

struct ConnectResult {
    std::string device_id;
    bool connected = false;
    std::vector<ConnectAttempt> attempts;  // full retry history, in order
    std::string session_token;             // non-empty iff connected
};

// Simulates a connect handshake. Deterministically seeded by device_id
// (NOT real randomness): devices whose id ends in an even number
// simulate exactly one timed-out attempt before succeeding on the
// second, to exercise the retry path; odd-numbered devices connect on
// the first attempt. Gives up (connected=false) if max_attempts is
// exhausted or the device id isn't in discover_devices()'s catalog.
ConnectResult connect(const DeviceDescriptor& device, int max_attempts = 3);

struct TelemetrySample {
    int sample_index = 0;
    double timestamp_s = 0.0;  // simulated session-relative time, not wall-clock
    double voltage_v = 0.0;
    double temperature_c = 0.0;
    bool clock_locked = true;
    std::string note;  // non-empty on a simulated dropout/recovery sample
};

struct TelemetrySession {
    std::string device_id;
    std::vector<TelemetrySample> samples;
    int dropout_count = 0;
    int recovery_count = 0;
    bool all_dropouts_recovered = true;
};

// Pulls `count` telemetry samples from an already-connected mock session.
// Deterministically seeded by device_id, so reproducible. For any session
// with count >= 4, simulates exactly one connection dropout at the
// midpoint sample, then an automatic reconnect (by calling connect()
// again internally) before resuming -- this is the module's
// timeout-recovery demonstration. If the automatic reconnect itself
// fails (which, given connect()'s deterministic scripting, only happens
// if max_attempts is set unreasonably low), sampling stops early and
// all_dropouts_recovered is false.
TelemetrySession read_telemetry(const ConnectResult& session, int count = 20);

struct PinInfo {
    std::string name;
    std::string function;
    double voltage_v = 0.0;
    std::string direction;  // "input" / "output" / "bidirectional" / "power" / "ground"
};

// Representative pin tables modeled loosely on common, publicly
// documented conventions for each device class (an 8-bit AVR-style MCU,
// a 7-series-style FPGA's power rails, and the standard 10-pin ARM JTAG
// header) -- not a reproduction of any single vendor's exact datasheet
// for a specific part number, since no specific real device is being
// claimed here.
std::vector<PinInfo> pinout(DeviceType type);

void export_telemetry_csv(const TelemetrySession& session, const std::string& path);
void export_pinout_csv(DeviceType type, const std::string& path);

}  // namespace nexus::hardware
