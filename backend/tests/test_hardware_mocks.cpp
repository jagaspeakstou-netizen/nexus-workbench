#include <gtest/gtest.h>

#include "nexus/hardware_mocks.hpp"

using namespace nexus::hardware;

TEST(HardwareDiscovery, ReturnsDistinctDeterministicCatalogPerDeviceType) {
    auto mcus = discover_devices(DeviceType::Microcontroller);
    auto fpgas = discover_devices(DeviceType::Fpga);
    auto jtags = discover_devices(DeviceType::Jtag);

    EXPECT_EQ(mcus.size(), 2u);
    EXPECT_EQ(fpgas.size(), 2u);
    EXPECT_EQ(jtags.size(), 2u);

    for (const auto& d : mcus) EXPECT_EQ(d.type, DeviceType::Microcontroller);
    for (const auto& d : fpgas) EXPECT_EQ(d.type, DeviceType::Fpga);
    for (const auto& d : jtags) EXPECT_EQ(d.type, DeviceType::Jtag);

    // Determinism: calling twice must return identical results (a mock
    // discovery scan has no bus to be nondeterministic about).
    auto mcus_again = discover_devices(DeviceType::Microcontroller);
    ASSERT_EQ(mcus.size(), mcus_again.size());
    for (size_t i = 0; i < mcus.size(); ++i) {
        EXPECT_EQ(mcus[i].id, mcus_again[i].id);
    }
}

TEST(HardwareConnect, OddNumberedDeviceConnectsOnFirstAttempt) {
    auto devices = discover_devices(DeviceType::Microcontroller);
    ASSERT_FALSE(devices.empty());
    const auto& mcu01 = devices[0];  // "mcu-sim-01"
    ASSERT_EQ(mcu01.id, "mcu-sim-01");

    auto result = connect(mcu01);
    EXPECT_TRUE(result.connected);
    ASSERT_EQ(result.attempts.size(), 1u);
    EXPECT_EQ(result.attempts[0].outcome, ConnectOutcome::Connected);
    EXPECT_FALSE(result.session_token.empty());
}

TEST(HardwareConnect, EvenNumberedDeviceTimesOutOnceThenRecovers) {
    auto devices = discover_devices(DeviceType::Microcontroller);
    ASSERT_GE(devices.size(), 2u);
    const auto& mcu02 = devices[1];  // "mcu-sim-02"
    ASSERT_EQ(mcu02.id, "mcu-sim-02");

    auto result = connect(mcu02);
    EXPECT_TRUE(result.connected);
    ASSERT_EQ(result.attempts.size(), 2u);
    EXPECT_EQ(result.attempts[0].outcome, ConnectOutcome::TimedOut);
    EXPECT_EQ(result.attempts[1].outcome, ConnectOutcome::Connected);
}

TEST(HardwareConnect, GivesUpCleanlyWhenMaxAttemptsIsBelowWhatScriptedRetryNeeds) {
    auto devices = discover_devices(DeviceType::Fpga);
    const auto& fpga02 = devices[1];  // scripted to need 2 attempts
    ASSERT_EQ(fpga02.id, "fpga-sim-02");

    auto result = connect(fpga02, /*max_attempts=*/1);
    EXPECT_FALSE(result.connected);
    EXPECT_TRUE(result.session_token.empty());
    ASSERT_EQ(result.attempts.size(), 1u);
    EXPECT_EQ(result.attempts[0].outcome, ConnectOutcome::TimedOut);
}

TEST(HardwareConnect, IsDeterministicForTheSameDevice) {
    auto devices = discover_devices(DeviceType::Jtag);
    auto a = connect(devices[0]);
    auto b = connect(devices[0]);
    EXPECT_EQ(a.connected, b.connected);
    EXPECT_EQ(a.attempts.size(), b.attempts.size());
    for (size_t i = 0; i < a.attempts.size(); ++i) {
        EXPECT_EQ(a.attempts[i].outcome, b.attempts[i].outcome);
        EXPECT_DOUBLE_EQ(a.attempts[i].simulated_latency_ms, b.attempts[i].simulated_latency_ms);
    }
}

TEST(HardwareTelemetry, RefusesToSampleAnUnconnectedSession) {
    ConnectResult fake_session;
    fake_session.device_id = "mcu-sim-01";
    fake_session.connected = false;  // never actually connected

    auto telemetry = read_telemetry(fake_session, 20);
    EXPECT_TRUE(telemetry.samples.empty());
}

TEST(HardwareTelemetry, SimulatesExactlyOneDropoutAndRecoversForASufficientlyLongSession) {
    auto devices = discover_devices(DeviceType::Microcontroller);
    auto session = connect(devices[0]);
    ASSERT_TRUE(session.connected);

    const int count = 20;
    auto telemetry = read_telemetry(session, count);

    EXPECT_EQ(telemetry.dropout_count, 1);
    EXPECT_EQ(telemetry.recovery_count, 1);
    EXPECT_TRUE(telemetry.all_dropouts_recovered);
    EXPECT_EQ(telemetry.samples.size(), static_cast<size_t>(count));

    // Exactly one sample should carry a non-empty dropout/recovery note,
    // and it should be the midpoint sample (count/2), matching the
    // documented deterministic dropout rule.
    int notes_count = 0;
    int dropout_sample_index = -1;
    for (const auto& s : telemetry.samples) {
        if (!s.note.empty()) {
            notes_count += 1;
            dropout_sample_index = s.sample_index;
        }
    }
    EXPECT_EQ(notes_count, 1);
    EXPECT_EQ(dropout_sample_index, count / 2);
}

TEST(HardwareTelemetry, NoDropoutSimulatedForAShortSession) {
    auto devices = discover_devices(DeviceType::Jtag);
    auto session = connect(devices[0]);
    ASSERT_TRUE(session.connected);

    auto telemetry = read_telemetry(session, /*count=*/3);  // below the dropout threshold
    EXPECT_EQ(telemetry.dropout_count, 0);
    EXPECT_EQ(telemetry.samples.size(), 3u);
    for (const auto& s : telemetry.samples) {
        EXPECT_TRUE(s.note.empty());
        EXPECT_TRUE(s.clock_locked);
    }
}

TEST(HardwareTelemetry, VoltageAndTemperatureStayWithinASaneSimulatedRange) {
    auto devices = discover_devices(DeviceType::Fpga);
    auto session = connect(devices[0]);
    ASSERT_TRUE(session.connected);
    auto telemetry = read_telemetry(session, 20);

    for (const auto& s : telemetry.samples) {
        if (!s.note.empty()) continue;  // skip the dropout placeholder sample
        EXPECT_GT(s.voltage_v, 2.5);
        EXPECT_LT(s.voltage_v, 4.0);
        EXPECT_GT(s.temperature_c, 15.0);
        EXPECT_LT(s.temperature_c, 50.0);
    }
}

TEST(HardwareTelemetry, IsDeterministicForTheSameConnectedSession) {
    auto devices = discover_devices(DeviceType::Microcontroller);
    auto session = connect(devices[0]);
    auto a = read_telemetry(session, 20);
    auto b = read_telemetry(session, 20);
    ASSERT_EQ(a.samples.size(), b.samples.size());
    for (size_t i = 0; i < a.samples.size(); ++i) {
        EXPECT_DOUBLE_EQ(a.samples[i].voltage_v, b.samples[i].voltage_v);
        EXPECT_DOUBLE_EQ(a.samples[i].temperature_c, b.samples[i].temperature_c);
    }
}

TEST(HardwarePinout, EachDeviceTypeHasANonEmptyPinTableWithPowerAndGround) {
    for (DeviceType t : {DeviceType::Microcontroller, DeviceType::Fpga, DeviceType::Jtag}) {
        auto pins = pinout(t);
        EXPECT_FALSE(pins.empty());
        bool has_power_or_reference = false;
        bool has_ground = false;
        for (const auto& p : pins) {
            if (p.direction == "power" || p.name == "VTREF") has_power_or_reference = true;
            if (p.direction == "ground") has_ground = true;
        }
        EXPECT_TRUE(has_power_or_reference) << "device type " << to_string(t);
        EXPECT_TRUE(has_ground) << "device type " << to_string(t);
    }
}
