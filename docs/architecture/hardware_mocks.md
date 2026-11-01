# Hardware Integration module — mock adapters

Implements `backend/include/nexus/hardware_mocks.hpp` /
`backend/src/hardware/hardware_mocks.cpp`.

## What this is, and what it is not

**There is no physical hardware attached to this project.** No
microcontroller, FPGA, or JTAG probe was connected to the machine this
was developed on. Everything this module reports — device discovery,
connect handshakes, voltage/temperature telemetry — comes from a
deterministic software simulator written specifically for this
assignment's adapter-pattern requirement (discovery / connect / telemetry
/ timeout-recovery), not from a real device driver with the device
missing.

This is disclosed everywhere the data surfaces: the API response's own
`mock_disclaimer` field, the frontend page's banner, the header comment at
the top of `hardware_mocks.hpp`, and this document's title. None of it is
presented as, or could be mistaken for, a genuine hardware readout.

**What is real:** the adapter *logic*. A real adapter for any of these
three device classes would need the same shape — enumerate reachable
devices, open a session with retry/backoff on failure, poll telemetry,
detect and recover from a dropped link — and this module implements that
shape completely and testably, with the "device" being a deterministic
stand-in rather than a physical one. `test_hardware_mocks.cpp`'s 10 test
cases exercise the actual logic (retry counts, dropout/recovery
bookkeeping, determinism, pin table completeness), not placeholder
assertions.

## Why "simulated" instead of randomized

A naive mock might use real randomness (`std::random_device`, wall-clock
seeding) to vary its behavior run to run. This module deliberately does
not: every random-looking value — connect latency, voltage noise,
temperature noise — is seeded from a hash of the device id (`connect()`)
or device id + session token (`read_telemetry()`), via `std::mt19937`, the
same deterministic-PRNG pattern already used in the Performance Lab
module's case generator. Two consequences:

1. **Tests are deterministic.** `HardwareConnect.IsDeterministicForTheSameDevice`
   and `HardwareTelemetry.IsDeterministicForTheSameConnectedSession` would
   be flaky with real randomness; with seeded randomness they pass every
   time, by construction.
2. **The retry/recovery paths are reachable on demand, not by chance.**
   Devices whose id ends in an even digit (`mcu-sim-02`, `fpga-sim-02`,
   `jtag-sim-02`) are scripted to time out exactly once before connecting
   on the second attempt; odd-numbered devices connect on the first
   attempt. A long-enough telemetry session (`count >= 4`) always
   simulates exactly one dropout at its midpoint sample, followed by an
   automatic reconnect. This is why the module can be demonstrated and
   tested without ever needing to get lucky.

## Block diagram

```
 discover_devices()  --->   connect()    --->  read_telemetry()  --->  export_*_csv()
  fixed mock catalog      scripted retry        sampling loop          submission evidence
                              ^   |                  ^   |
                              |   | (timeout)         |   | (dropout at
                              +---+                   |   |  session midpoint)
                                                       |   v
                                                  (re-run connect(), then resume)
                                                       +---+
```

(rendered as an inline SVG on the Hardware Integration page, same content)

- **`discover_devices(type)`** returns a fixed catalog of 2 mock devices
  per type (6 total: 2 microcontroller, 2 FPGA, 2 JTAG probe) — pure
  function of `type`, no state, no I/O.
- **`connect(device, max_attempts)`** runs the scripted retry/backoff
  handshake described above, returning every attempt's outcome and
  simulated latency, not just the final result.
- **`read_telemetry(session, count)`** samples `count` points, injecting
  exactly one simulated dropout (for `count >= 4`) at the midpoint, which
  triggers an internal call back into `connect()` to simulate automatic
  recovery — the same function a real reconnect would call, not a
  separate code path that only pretends to recover.
- **`export_telemetry_csv()` / `export_pinout_csv()`** write the raw
  evidence committed under `results/csv/`.

## Pin / voltage tables

Three representative tables (`pinout(DeviceType)`), modeled on commonly
documented conventions for each device class rather than reproducing any
single vendor's exact datasheet for a specific part number (no specific
real device is being claimed):

- **Microcontroller** — loosely modeled on common 8-bit AVR conventions:
  5V supply, UART RX/TX, a GPIO port bank, ADC-capable pins, AREF.
- **FPGA** — loosely modeled on common 7-series-class power rail and JTAG
  configuration conventions: separate VCCINT (core, 1.0V) / VCCAUX
  (1.8V) / VCCO (bank I/O, 3.3V) supplies, the 4-wire JTAG bus (TDI/TDO/
  TCK/TMS), and the PROG_B/DONE/INIT_B configuration-status pins.
- **JTAG probe** — the standard, publicly documented 10-pin Cortex debug
  connector pinout (VTREF, TMS/SWDIO, TCK/SWCLK, TDO/SWO, TDI, nRESET,
  grounds, key pin) — used because it's the de facto industry standard
  this kind of probe models, independent of any specific vendor.

Full tables: see the Hardware Integration page, or
`results/csv/hardware_pinout_{microcontroller,fpga,jtag}.csv`.

## Timeout-recovery in detail

The one behavior worth calling out explicitly, since it's the module's
namesake requirement: `read_telemetry()` does not special-case the
dropout as "skip this sample and carry on." It:

1. Records a `TelemetrySample` at the dropout index with `clock_locked =
   false` and a note describing what happened, instead of silently
   omitting that sample.
2. Calls `connect()` again, for real — reusing the exact same
   scripted-retry logic a fresh connection would use, looked up from
   `discover_devices()` by id (not a hand-constructed shortcut).
3. Only resumes normal sampling if that reconnect succeeds; if it didn't
   (achievable only by passing an unreasonably low `max_attempts`),
   sampling stops early and `all_dropouts_recovered` is `false` — the
   module can represent and report a failed recovery, not just simulate
   success unconditionally.

`test_hardware_mocks.cpp`'s
`HardwareTelemetry.SimulatesExactlyOneDropoutAndRecoversForASufficientlyLongSession`
and `HardwareConnect.GivesUpCleanlyWhenMaxAttemptsIsBelowWhatScriptedRetryNeeds`
cover both the success and failure paths.

## Reproducing

```
cd backend/build
./nexus_hardware_export ../../results/csv   # regenerates the CSVs described above
cd .. && ctest --output-on-failure -R Hardware
```
