#include "nexus/hardware_mocks.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <stdexcept>

namespace nexus::hardware {

std::string to_string(DeviceType type) {
    switch (type) {
        case DeviceType::Microcontroller: return "microcontroller";
        case DeviceType::Fpga: return "fpga";
        case DeviceType::Jtag: return "jtag";
    }
    return "unknown";
}

namespace {

// Deterministic per-device seed: a simple string hash, not
// std::random_device -- this project needs reproducible mock behavior,
// never real entropy.
unsigned seed_from_id(const std::string& id) {
    unsigned h = 2166136261u;  // FNV-1a
    for (char c : id) {
        h ^= static_cast<unsigned char>(c);
        h *= 16777619u;
    }
    return h;
}

// True for device ids ending in an even digit -- used to pick which mock
// devices simulate a retry-then-succeed connect handshake. Documented in
// the header as the deterministic rule it is.
bool id_ends_in_even_digit(const std::string& id) {
    if (id.empty() || !std::isdigit(static_cast<unsigned char>(id.back()))) {
        return false;
    }
    return (id.back() - '0') % 2 == 0;
}

}  // namespace

std::vector<DeviceDescriptor> discover_devices(DeviceType type) {
    switch (type) {
        case DeviceType::Microcontroller:
            return {
                {"mcu-sim-01", DeviceType::Microcontroller, "Mock Semiconductor Co.",
                 "8-bit AVR-class MCU (simulated)", "UART (simulated, mock-COM3)"},
                {"mcu-sim-02", DeviceType::Microcontroller, "Mock Semiconductor Co.",
                 "32-bit Cortex-M-class MCU (simulated)", "USB-CDC (simulated, mock-ttyACM0)"},
            };
        case DeviceType::Fpga:
            return {
                {"fpga-sim-01", DeviceType::Fpga, "Mock Logic Devices Inc.",
                 "7-series-class FPGA (simulated)", "JTAG (simulated, mock-chain-0)"},
                {"fpga-sim-02", DeviceType::Fpga, "Mock Logic Devices Inc.",
                 "UltraScale-class FPGA (simulated)", "PCIe (simulated, mock-pcie0)"},
            };
        case DeviceType::Jtag:
            return {
                {"jtag-sim-01", DeviceType::Jtag, "Mock Debug Tools Ltd.",
                 "10-pin ARM JTAG probe (simulated)", "USB (simulated, mock-jtag-probe-A)"},
                {"jtag-sim-02", DeviceType::Jtag, "Mock Debug Tools Ltd.",
                 "20-pin JTAG/SWD probe (simulated)", "USB (simulated, mock-jtag-probe-B)"},
            };
    }
    return {};
}

ConnectResult connect(const DeviceDescriptor& device, int max_attempts) {
    ConnectResult result;
    result.device_id = device.id;

    std::mt19937 rng(seed_from_id(device.id));
    std::uniform_real_distribution<double> latency_dist(8.0, 35.0);

    const bool scripted_first_timeout = id_ends_in_even_digit(device.id);

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        ConnectAttempt a;
        a.attempt_number = attempt;
        a.simulated_latency_ms = latency_dist(rng);

        const bool this_attempt_times_out = scripted_first_timeout && attempt == 1;
        a.outcome = this_attempt_times_out ? ConnectOutcome::TimedOut : ConnectOutcome::Connected;
        result.attempts.push_back(a);

        if (a.outcome == ConnectOutcome::Connected) {
            result.connected = true;
            result.session_token = device.id + "-session-" + std::to_string(seed_from_id(device.id) % 100000);
            return result;
        }
    }

    result.connected = false;
    return result;
}

TelemetrySession read_telemetry(const ConnectResult& session, int count) {
    TelemetrySession out;
    out.device_id = session.device_id;

    if (!session.connected || count <= 0) {
        out.all_dropouts_recovered = (count <= 0);  // nothing to recover if nothing was requested
        return out;
    }

    std::mt19937 rng(seed_from_id(session.device_id + session.session_token));
    std::normal_distribution<double> voltage_noise(0.0, 0.015);
    std::normal_distribution<double> temp_noise(0.0, 0.3);

    const double nominal_voltage = 3.3;
    const double nominal_temp_c = 32.0;
    const double dt = 0.05;  // simulated seconds per sample

    const int dropout_index = (count >= 4) ? (count / 2) : -1;
    int current_sample = 0;
    double t = 0.0;

    // Current "live" device descriptor, needed only to re-run connect()
    // for the simulated reconnect -- looked up from discover_devices()
    // by id, not reconstructed by hand, so it stays consistent with the
    // catalog.
    DeviceDescriptor active_device;
    bool found = false;
    for (DeviceType candidate_type : {DeviceType::Microcontroller, DeviceType::Fpga, DeviceType::Jtag}) {
        for (const auto& d : discover_devices(candidate_type)) {
            if (d.id == session.device_id) {
                active_device = d;
                found = true;
            }
        }
    }

    while (current_sample < count) {
        if (current_sample == dropout_index) {
            // Simulated dropout: this sample is lost (not recorded), and
            // an automatic reconnect is attempted immediately using the
            // same connect() logic a fresh session would use.
            out.dropout_count += 1;

            ConnectResult reconnect_result;
            if (found) {
                reconnect_result = connect(active_device, /*max_attempts=*/3);
            }

            TelemetrySample dropout_sample;
            dropout_sample.sample_index = current_sample;
            dropout_sample.timestamp_s = t;
            dropout_sample.clock_locked = false;
            dropout_sample.note = reconnect_result.connected
                ? "simulated dropout at this sample; automatic reconnect succeeded ("
                      + std::to_string(reconnect_result.attempts.size()) + " attempt(s))"
                : "simulated dropout at this sample; automatic reconnect FAILED";
            out.samples.push_back(dropout_sample);

            if (reconnect_result.connected) {
                out.recovery_count += 1;
            } else {
                out.all_dropouts_recovered = false;
                break;  // can't continue sampling without a live session
            }

            current_sample += 1;
            t += dt;
            continue;
        }

        TelemetrySample s;
        s.sample_index = current_sample;
        s.timestamp_s = t;
        s.voltage_v = nominal_voltage + 0.05 * std::sin(t * 6.0) + voltage_noise(rng);
        s.temperature_c = nominal_temp_c + 1.5 * std::sin(t * 1.5) + temp_noise(rng);
        s.clock_locked = true;
        out.samples.push_back(s);

        current_sample += 1;
        t += dt;
    }

    return out;
}

std::vector<PinInfo> pinout(DeviceType type) {
    switch (type) {
        case DeviceType::Microcontroller:
            return {
                {"VCC", "Supply voltage", 5.0, "power"},
                {"GND", "Ground", 0.0, "ground"},
                {"RESET", "Active-low reset", 5.0, "input"},
                {"PB0-PB7", "Port B general-purpose I/O (one bank)", 5.0, "bidirectional"},
                {"PC0-PC5", "Port C / ADC input channels", 5.0, "bidirectional"},
                {"PD0 (RXD)", "UART receive", 5.0, "input"},
                {"PD1 (TXD)", "UART transmit", 5.0, "output"},
                {"AVCC", "Analog supply (ADC)", 5.0, "power"},
                {"AREF", "ADC reference voltage", 5.0, "input"},
            };
        case DeviceType::Fpga:
            return {
                {"VCCINT", "Core logic supply", 1.0, "power"},
                {"VCCAUX", "Auxiliary supply (config, clocking)", 1.8, "power"},
                {"VCCO_33", "Bank I/O supply (3.3V bank)", 3.3, "power"},
                {"GND", "Ground", 0.0, "ground"},
                {"TDI", "JTAG test data in", 3.3, "input"},
                {"TDO", "JTAG test data out", 3.3, "output"},
                {"TCK", "JTAG test clock", 3.3, "input"},
                {"TMS", "JTAG test mode select", 3.3, "input"},
                {"PROG_B", "Active-low configuration reset", 3.3, "input"},
                {"DONE", "Configuration-complete status", 3.3, "output"},
                {"INIT_B", "Configuration init/error status", 3.3, "bidirectional"},
            };
        case DeviceType::Jtag:
            // Standard 10-pin Cortex debug connector pinout (publicly
            // documented, ARM-defined convention) -- used here because
            // it's the de facto standard this mock probe models, not
            // because it belongs to any specific vendor's product.
            return {
                {"VTREF", "Target reference voltage (sense, not supply)", 3.3, "input"},
                {"TMS/SWDIO", "Test mode select / SWD data", 3.3, "bidirectional"},
                {"GND", "Ground", 0.0, "ground"},
                {"TCK/SWCLK", "Test clock / SWD clock", 3.3, "input"},
                {"GND", "Ground", 0.0, "ground"},
                {"TDO/SWO", "Test data out / Serial wire output", 3.3, "output"},
                {"NC/KEY", "Key pin (no connect)", 0.0, "ground"},
                {"TDI", "Test data in", 3.3, "input"},
                {"nRESET", "Target reset", 3.3, "bidirectional"},
            };
    }
    return {};
}

void export_telemetry_csv(const TelemetrySession& session, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("hardware::export_telemetry_csv: could not open " + path);
    }
    out << "sample_index,timestamp_s,voltage_v,temperature_c,clock_locked,note\n";
    for (const auto& s : session.samples) {
        out << s.sample_index << ',' << s.timestamp_s << ',' << s.voltage_v << ','
            << s.temperature_c << ',' << (s.clock_locked ? "true" : "false") << ','
            << '"' << s.note << '"' << '\n';
    }
}

void export_pinout_csv(DeviceType type, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("hardware::export_pinout_csv: could not open " + path);
    }
    out << "device_type,name,function,voltage_v,direction\n";
    for (const auto& p : pinout(type)) {
        out << to_string(type) << ',' << p.name << ",\"" << p.function << "\"," << p.voltage_v
            << ',' << p.direction << '\n';
    }
}

}  // namespace nexus::hardware
