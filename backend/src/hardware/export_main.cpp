// nexus_hardware_export <output_dir>
//
// Runs the mock discovery/connect/telemetry sequence for one device of
// each type and writes CSV evidence: telemetry traces (with the
// scripted dropout/recovery visible in the notes column) and the three
// pin tables.

#include <cstdio>
#include <string>

#include "nexus/hardware_mocks.hpp"

namespace {

void run_device_type(nexus::hardware::DeviceType type, const std::string& output_dir) {
    using namespace nexus::hardware;
    auto devices = discover_devices(type);
    std::printf("== %s: %zu mock device(s) discovered ==\n", to_string(type).c_str(), devices.size());

    for (const auto& device : devices) {
        std::printf("  %s (%s) via %s\n", device.id.c_str(), device.label.c_str(), device.interface.c_str());
        auto session = connect(device);
        std::printf("    connect: %s after %zu attempt(s)\n",
                     session.connected ? "OK" : "FAILED", session.attempts.size());
        if (!session.connected) continue;

        auto telemetry = read_telemetry(session, 20);
        std::printf("    telemetry: %zu samples, %d dropout(s), %d recovery(ies), all_recovered=%s\n",
                     telemetry.samples.size(), telemetry.dropout_count, telemetry.recovery_count,
                     telemetry.all_dropouts_recovered ? "true" : "false");

        const std::string path = output_dir + "/hardware_telemetry_" + device.id + ".csv";
        export_telemetry_csv(telemetry, path);
        std::printf("    wrote %s\n", path.c_str());
    }

    const std::string pin_path = output_dir + "/hardware_pinout_" + to_string(type) + ".csv";
    export_pinout_csv(type, pin_path);
    std::printf("  wrote %s\n", pin_path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    std::string output_dir = (argc > 1) ? argv[1] : ".";
    using nexus::hardware::DeviceType;
    run_device_type(DeviceType::Microcontroller, output_dir);
    run_device_type(DeviceType::Fpga, output_dir);
    run_device_type(DeviceType::Jtag, output_dir);
    return 0;
}
