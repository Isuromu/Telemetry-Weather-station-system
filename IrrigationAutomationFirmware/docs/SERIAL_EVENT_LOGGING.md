# Serial event logging

Status of the "print each event to serial" pass across the LoRa examples. PumpNode
was done on 2026-10-07, including a consistent `[SUBSYSTEM]` tag on every line.
Still open: payload hex on MainValve, and the state-change pass on Valve1, Valve2,
WaterLevel and SoilNode. The four questions raised on 2026-10-07 are answered below,
and the payload-hex switch is decided: compile-time, default on.

Next session: build the two logging libraries (`EventLog` = what the line says,
`LogSink` = where the bytes go), migrate the examples onto them across five commits —
the tag-abbreviation pass first, then the library work — and tag MainValve. The
decisions already taken, the recommended tag vocabulary and valve-node events, and the
commit plan are all below.

## What counts as an event

An event line explains what the node did, as opposed to a periodic measurement
dump. The set the examples converge on:

- **LoRaWAN frames as hex**, before sending and after receiving:
  `[LORAWAN] Status FPort 31: 02 FF 02 ...`, `[LORAWAN] Downlink: 01 04 00 05 00 00`.
- **Radio path outcomes**: join attempt, join failure, session established vs
  restored, Class C activation, uplink sent, uplink failed, Class C downlink received.
- **Command outcomes**: accepted, or one line naming why it was rejected — including
  the protocol-level rejections that only set a result code.
- **State changes**, printed once per change and not once per poll or wake, by
  keeping the last reported value and comparing.

Conventions to keep:

- `[SUBSYSTEM]` prefix, `[SUBSYSTEM][ERROR]` for failures. In use: `[SYSTEM]`,
  `[LORAWAN]`, `[PUMP]`, `[VFD]`, `[POWER]`, `[SERIAL]`, `[SERIAL AUTH]`,
  `[WIRELESS AUTH]`, `[MODBUS]`, `[FLOW]`, `[SOIL]`, `[BATTERY]`.
- Numeric diagnostics use `meaning [code]`, never a bare number
  (`README.md:133-138`, `AGENTS.md:48-50`), via
  `irrigation::diagnostics::radioErrorMeaning()` for RadioLib codes.
- **Timestamps are not firmware.** `platformio.ini:9` sets `monitor_filters = time`,
  so the host monitor adds the clock prefix. No example has, or needs, a
  timestamping `Print` layer.
- **Tags are short, and defined once.** Decided 2026-10-07: the user wants
  abbreviated tags. It is *not* a Flash saving — tags cost a few bytes per distinct
  format string, on the order of a kilobyte, against ~142 KB free on the pump and the
  ~776 KB `PUMP_BT_CONSOLE=0` frees (`examples/PumpControl/CHANGELOG.md:18-19`), and
  literals are flash-mapped so RAM is untouched either way. The driver is legibility
  and line width. Do it as one vocabulary pass, below, before the `EventLog` work.

## Where the lines go

Every example except PumpControl prints to USB only. PumpControl also mirrors the
same stream to a Bluetooth Classic SPP console
(`examples/PumpControl/include/BluetoothConsole.h`), because VFD and motor noise
drop its USB-UART link while the pump runs. It is the only node with that console,
and BLE is explicitly disabled there, so it is paired with an SPP terminal. New
lines should keep writing through `Serial` / the `logTee` so they reach both
channels without extra work.

A third channel — WiFi, readable on the laptop for all six nodes — is planned in
[`WIRELESS_DEBUG_PLAN.md`](WIRELESS_DEBUG_PLAN.md). It is one more `LogSink`
implementation plus a host collector, so the vocabulary and the `EventLog`/`LogSink`
split above are unchanged by it.

## Where each example stands

| Example | Frame hex (up) | Frame hex (down) | Session new/restored | Command outcomes | State changes |
|---|---|---|---|---|---|
| PumpControl | yes (2026-10-07) | yes (2026-10-07) | yes | yes (2026-10-07) | yes (2026-10-07) |
| PressureControlNode | yes | yes | yes | yes | no |
| PressureControlNode2 | yes | yes | yes | yes | no |
| MainValve | no | no | yes | yes | yes (pressure read) |
| WaterLevel | yes | **no** | partial | n/a (no commands) | **no** |
| SoilNode | yes | yes | partial | yes | **no** |

Reference implementation for the change-gated print is
`examples/MainValve/src/main.cpp:665-674` (retained `lastPressureReadStatus`, comment
"Print only on a change of state"). PumpControl's version is
`logStatusChanges()` in `examples/PumpControl/src/main.cpp`, driven from `loop()`
next to the existing `statusSignature()` comparison.

## Pending: WaterLevel

`examples/WaterLevel/src/main.cpp`, env `water_level`, Class A deep-sleep node.

1. **Downlink hex.** The text-only line at `:374-377`
   (`[LORAWAN] Ignored %u-byte application downlink on FPort %u; remote control is
   intentionally disabled.`) should dump the bytes too, using the existing
   `printHexFrame` (`:342-350`) with a `"[LORAWAN] Downlink: "` label. Note that
   helper hard-codes its label; give it a label parameter as the PCV/SoilNode copies
   already do.
2. **Session line.** `:338` prints `[LORAWAN] Class A session ready.` for both cases.
   Split it into new vs restored, as MainValve does at `:1075-1077`
   ("New LoRaWAN session established." / "LoRaWAN session restored.").
3. **Class-select meaning.** `:335` `[LORAWAN] Could not select Class A: %d` logs a
   bare number, against the convention. Use `radioErrorMeaning(state)` plus the code.
4. **State changes.** Today `LOAD ON` / `LOAD OFF: battery or water level is low`
   (`:194-195`) prints every cycle, and battery/sensor failures print with them
   (`:218`, `:234-242`). Add a retained snapshot (`loadState`, battery-valid,
   transmitter-valid, and optionally the depth band) and print one line per change.
   Deep sleep loses RAM, so on a Class A node the snapshot only covers one wake —
   either accept one line per wake or persist the last state in RTC memory, which is
   what the PCV nodes do for their session.

## Pending: SoilNode

`examples/SoilNode/src/main.cpp`, env `soil_node`, Class A deep-sleep node.

1. **Session line.** Same split as WaterLevel: `:363` `[LORAWAN] Class A session
   ready.` should say new or restored.
2. **Class-select meaning.** `:360` `[LORAWAN] Could not select Class A: %d` — same
   fix as WaterLevel.
3. **State changes.** The wake banner and `Wake count` already print (`:448-449`).
   Missing are change lines for sensor validity (a CRC-valid reply was not found,
   `:185-188`), battery/ADC state (`:202`, `:455`), and the reported values moving
   out of their valid band. Same retained-snapshot caveat as WaterLevel.
4. **Frames already covered** (`:386`, `:401`, `:406`), no work needed.

## Deferred by the user

Grouping each line by where the command came from (`[LoRaWAN]`, `[USB]`,
`[Bluetooth]`) is deliberately not implemented. The existing tags name the
subsystem that produced the line, not the command source.

## Planned: LoRaWAN payload hex on Valve1 and Valve2

Requested 2026-10-07. **Status: already implemented.** Both nodes dump their
FPort 31 status, FPort 31 command acknowledgement, FPort 31 downlink, and the
Class C downlink as hex through a local `printHexFrame`:

- `examples/PressureControlNode/src/main.cpp` — helper `:287`, calls `:581` (status),
  `:607` (downlink), `:617` (ack), `:784` (Class C downlink).
- `examples/PressureControlNode2/src/main.cpp` — helper `:278`, calls `:542`, `:568`,
  `:578`, `:732`.

So the open question was not whether to print, but whether it stays unconditional.
**Decided 2026-10-07: switchable, compile-time** (see "Decisions taken" above); the
shared helper gains the flag in commit 2. Options considered:

1. Leave it always on (today's behaviour).
2. Compile-time flag, the `PUMP_BT_CONSOLE` pattern (`#ifndef` in the example header
   plus a build flag in `platformio.ini`). Works on both builds. A runtime toggle is
   unreachable on the Class A build anyway, which prints
   `[SERIAL] Diagnostic output only; Serial commands are disabled.`
3. Runtime gate on `irrigation::serial_debug::full()`, which already gates other
   verbose output (`lib/Tuf2000mFlowMeter/src/Tuf2000mFlowMeter.cpp:47`). Useful on
   the Class C hybrid build, which keeps its command set; useless on Class A.

Keeping it on costs one extra line per wake — a few ms before the uplink — on top of
the boot banner the node already reprints on every wake.

## Planned: LoRaWAN payload hex on MainValve

Requested 2026-10-07. MainValve prints **no** payload hex: verified, the file has no
`printHexFrame` and no `HEX` use. To add:

- Port the helper from `examples/PressureControlNode/src/main.cpp:287-295` (the copy
  that takes a label).
- Uplink: dump the 18-byte FPort 31 status where it is built, labelled
  `[LORAWAN] Status FPort 31: `.
- Downlink: dump the 6-byte FPort 30 command where it is received, near the existing
  `Invalid downlink: FPort=%u length=%u.` line (`examples/MainValve/src/main.cpp:939`),
  labelled `[LORAWAN] Downlink: `.
- MainValve has no `PrintController` and prints through raw `Serial` (= `serialAccess`)
  as plain text with none of the `[SUBSYSTEM]` tags. Decide whether the new hex lines
  adopt `[LORAWAN]` like every other example or match MainValve's untagged style.
- The switchability question above applies here too.

## Planned: state-change pass on Valve1 and Valve2

Requested 2026-10-07. Neither PCV node has a change-gated print. What changes
between wakes — PCV position, `pcv_last_commanded`, `status_reason`, pressure, flow
total — is reported either in the FPort 31 status hex or in the periodic
`commands.printFlow(status.flow)` dump (`examples/PressureControlNode/src/main.cpp:816`),
printed every wake regardless of whether anything moved.

Both nodes already retain state across deep sleep in `RTC_DATA_ATTR
PersistentNodeState retainedNodeState` (`PressureControlNode/src/main.cpp:154`,
`PressureControlNode2/src/main.cpp:148`). A change line means extending that struct
with the fields to watch and comparing before printing, in the same shape as
PumpControl's `logStatusChanges()`. A plain RAM `lastValue` will not work: the node
reboots every wake, so it would reprint on each boot rather than on each change.

Decide first what counts as an event on a valve node — the commanded position
changing, the measured position changing, or a fault/reason code changing — because
printing all three restates most of the status frame that already follows as hex.

## Decisions taken 2026-10-07 (user)

- **Payload hex is switchable.** Compile-time, not runtime: a Class A node has no
  console to flip a runtime switch, and switchability exists mainly for the battery
  build. Plan: one macro in the library, default on, and a deployment env that sets
  it to 0 so the helper compiles to nothing.

## Recommended tag vocabulary for MainValve

MainValve's messages fall into six families. Proposed tags, chosen to match the
prefixes already in use elsewhere:

| Tag | MainValve messages today |
|---|---|
| `[SYSTEM]` | boot banner, `Commands:` help, `Type: help`, placeholder credentials |
| `[ACTUATOR]` | actual/target position, bus mode, `Target set: …`, movement rejections, calibration, locator, safe-target writes, fault clear/reset |
| `[PRESSURE]` | the pressure line, `Pressure read unavailable: …`, I2C recovery |
| `[LORAWAN]` | session established/restored, Class C activation, uplink sent/failed, Class-C downlink, invalid downlink, duplicate/reused command ID |
| `[CMD]` | serial command results (`Invalid angle; no movement performed.`, `Unknown command. Type help.`) |
| `[MODBUS]` | actuator Modbus exception and status lines |

## Required: command source on every command-driven line (all devices)

Requested 2026-10-07: when a device receives a command, the log must show where it
came from — USB, LoRaWAN or Bluetooth — and so must every line that command causes,
including its reply and the state changes that follow. Applies to every device, now
and in future.

Proposed vocabulary, matching prefixes already in use:

| Source | Tag | Existing precedent |
|---|---|---|
| USB console | `[SERIAL]` | `[SERIAL] Diagnostic output only; …` |
| Bluetooth SPP console | `[WIRELESS]` | `[WIRELESS AUTH]` |
| LoRaWAN downlink | `[LORAWAN]` | used throughout |
| Physical/manual (pin, button) | `[LOCAL]` | new |
| No command (boot, spontaneous) | the subsystem tag, as today | — |

Mechanism:

- **Immediate replies need no latch.** A command handled in the same call that
  delivered it — every serial-console command, and every LoRaWAN downlink — names its
  source directly, because the channel that invoked the handler is right there.
- **Deferred effects are attributed per command**, through the pending-command record
  the node already keeps (`pendingRemoteCompletion` + `lastCommandId` on the pump, the
  command phase on MainValve, `retainedNodeState` on the PCV nodes). That record gains
  a `source` field, so the completion, the refusal, and the state changes caused by
  command *N* carry the source of command *N* — correctly, even when command *N+1*
  arrived from another channel in the meantime.
- **Lines with no command behind them carry no source.** A VFD fault appearing on its
  own, or the AUTO/MANUAL pin moving, keeps only its subsystem tag and never inherits
  a stale source. That is what makes the attribution trustworthy: the source dies with
  the command record when the command completes, is superseded, or times out.
- Where a node tracks only the newest command, the newest wins — which matches the
  pump's existing behaviour of superseding its pending completion.
- On a deep-sleep node the record must survive sleep. The PCV nodes already keep it in
  `RTC_DATA_ATTR retainedNodeState`, so a command applied on the next wake still names
  the source that queued it.

Why this matters more than it looks: PumpControl's `LogTee` mirrors every line to USB
*and* Bluetooth, so when a command arrives on one channel and the reply appears on
both, the source tag is the only thing that says where it came from.

### Decided 2026-10-07

- **The source sits beside the subsystem tag**, in the slot the repo already uses for
  modifiers, so a line has one of these shapes:

  ```
  [SUBSYSTEM]                 no command behind the line
  [SUBSYSTEM][SOURCE]         command-driven
  [SUBSYSTEM][SOURCE][ERROR]  command-driven failure
  ```

  For example `[PUMP][WIRELESS] Start command accepted (id 292).`,
  `[PUMP][LORAWAN] Run state changed to forward.`,
  `[PUMP][LORAWAN][ERROR] Start blocked: set an explicit frequency after boot.`
  Existing two-part lines such as `[LORAWAN][ERROR] Status uplink failed: …` keep
  their meaning: subsystem first, level second.
- **Replies keep fanning out to every channel**, so each console stays a complete
  mirror of the log; the source tag is what tells you which channel asked.
- **The tag is `[WIRELESS]`**, deliberately general because a Wi-Fi console may join
  Bluetooth later. If the two ever need telling apart, that becomes a second value in
  the same slot rather than a rename.
- **On a command-driven line, slot 1 names what was commanded, not the channel.**
  Found in a live pump capture (2026-10-07): today's
  `[LORAWAN] Command set_frequency id=301 accepted.` would become
  `[LORAWAN][LOR] …`, where the source merely repeats the subsystem. So the
  command-outcome and command-effect lines move to the commanded subsystem and keep
  the source beside it: `[PUMP][LOR] Command set_frequency id=301 accepted.`,
  `[PUMP][SER] …`, `[ACT][LOR] Target set: 90.0 degrees.`, `[PCV][WLS] …`.
  Lines that no command caused keep their natural subsystem, so `[LORAWAN] Status
  uplink sent.` and the frame dumps are unchanged.

## Recommended valve-node events

From the valve node's own status fields (`pcv_low_power_class_a_codec.js:48-75`),
the discrete ones worth a line:

- `pcv_last_commanded` changes — the valve's commanded state.
- `status_reason` changes — why the last cycle acted (`remote_open_applied`,
  `duplicate_command_ignored`, `flow_total_reset_failed`, …).
- Pressure validity transitions — `upstream_pressure_bar` / `downstream_pressure_bar`
  going null ↔ a value (flag bits 0x01 and 0x02), i.e. a sensor lost or recovered.
- Battery validity transition (flag bit 0x04) and battery crossing a low threshold.
- valve_1 only: flow-total validity going valid ↔ the `0xFFFFFFFF` sentinel.

Not worth watching: the pressure values themselves (they drift every wake), the flow
totals (monotonic), and the sleep interval (only a command changes it, and the
command path already logs that).

## Recommended tracker design: per field, not whole-struct compare

Recommend a tiny watched-value type the node groups into one POD struct, so the same
struct can sit in RAM on a mains node or in `RTC_DATA_ATTR` on a deep-sleep node:

```cpp
template <typename T>
class Field {          // one watched value, integral or enum only
 public:
  bool changed(const T &now) {           // true the first time and on every change
    const bool differs = !valid_ || now != value_;
    value_ = now;
    valid_ = true;
    return differs;
  }
 private:
  T value_{};
  bool valid_{false};
};
```

Why not `memcmp` over a whole struct:

- **Padding.** Struct padding bytes are not initialised, so a blob compare reports a
  change every time even when no field moved — a false alarm at poll rate.
- **Floats.** Any float you watch drifts in the low bits, so a blob compare fires
  every cycle for changes nobody cares about. Restricting `Field<T>` to integral and
  enum types forces the call site to scale floats to hundredths, which is also what
  the payloads already transmit.
- **"Something changed" is not actionable.** A blob compare cannot say *what* moved,
  so the log either restates everything or tells you nothing — the noise this whole
  pass exists to remove.

## Planned: the shared logging libraries (next session)

Requested 2026-10-07 by the user: "I want one EventLog library." Motivated by the
duplication this document keeps recording — `printHexFrame` is copy-pasted into
PressureControlNode, PressureControlNode2, WaterLevel, SoilNode and PumpControl, and
change tracking is written three different ways (MainValve's `lastPressureReadStatus`,
PumpControl's `logStatusChanges()`, and nothing yet on the battery nodes).

Proposed shape, `lib/EventLog/` (`library.json`, `src/EventLog.h/.cpp`):

- `void printHexFrame(Print &out, const char *label, const uint8_t *data, size_t length)`
  — the exact per-byte zero-padded form the five copies already use, so call sites
  keep their current labels and output is unchanged.
- Tagged line emission with a level, so `[LORAWAN]` vs `[LORAWAN][ERROR]` vs
  `[LORAWAN][WARN]` is produced in one place:
  `void logLine(Print &out, const char *tag, LogLevel level, const char *format, ...)`.
- Change tracking over **caller-owned storage**: the helper holds the previous copy
  and a compare/update call, but the storage is passed in, so a mains node can keep
  it in RAM while a deep-sleep node keeps it in an `RTC_DATA_ATTR` global. That is
  the one requirement the current per-example code cannot share as-is.

Two things this deliberately does **not** become:

- Not part of `PrintController`. That class is a gate (`canPrint()` + pass-through);
  it has no access to payload bytes or previous values, and `lib/` components receive
  it precisely so they can print through it without knowing event vocabulary.
  `EventLog` takes a `Print&` and therefore composes with `SerialAccess` (MainValve),
  `PrintController` (PCV, pump) or `LogTee` (pump's USB + Bluetooth fan-out).
- Not a formatter that owns field sets. Each node keeps its own list of watched
  fields; the library only removes the mechanism.

Migration, smallest risk first: PumpControl and the two PCV nodes (already tagged,
just swap the helper), then WaterLevel and SoilNode (which also adopt the change
tracker while adding downlink hex), then MainValve (largest, because it has no tags
at all — see below).

Test without hardware: `test/test_protocol/` and `test/test_pressure_logic/` already
test header libraries with Unity plus `static_assert`. A `test/test_event_log/` suite
with an in-memory `Print` can assert the exact hex string and the exact
first-time/on-change/off-change behaviour of the tracker.

### Two libraries, split by the question they answer

The user's real complaint is recall: printing today is spread over names that are hard
to remember. Inventory of everything that touches logging:

| Today | What it is |
|---|---|
| `lib/SerialAccess/` (+ `include/SerialAuthConfig.h`, `config/SerialAuthSecrets.h`) | password gate on the USB stream |
| `lib/PrintController/` | gating wrapper; the type five libraries take as `logger_` |
| `lib/SerialDebugMode/` | `simple` / `full` verbosity flag |
| `examples/PumpControl/include/BluetoothConsole.h` | `LogTee` + `WirelessConsole` — the USB/Bluetooth fan-out, living inside an example |
| `printHexFrame` × 5 | one copy per example |
| `include/RadioErrorMeaning.h`, `include/ModbusErrorMeaning.h` | diagnostic `meaning [code]` formatters |

Recommended end state — two libraries, one name each, answering one question each:

1. **`lib/EventLog`** — *what the line says*: `line(tag, level, format, …)`,
   `hex(label, data, length)`, the `Field<T>` tracker, and the `meaning [code]`
   formatters moved in from `include/`.
2. **`lib/LogSink`** — *where the bytes go*: the single `Print` a node passes around,
   absorbing `SerialAccess` (auth), `LogTee` + `WirelessConsole` (USB + Bluetooth
   fan-out) and `SerialDebugMode` (verbosity). `PrintController` stays as the small
   interface the five libraries keep taking, so `lib/` consumers do not change.

Recall rule of thumb for the session notes: **EventLog is what, LogSink is where.**

### Five commits

0. **The vocabulary pass** described above: abbreviated tags across every example and
   library, with the docs updated in the same commit. A pure rename — every env still
   builds — and the only commit that must land before the rest.
1. **Add `lib/EventLog`** with `hex()`, `line()` and `Field<T>`, the command-source
   context described above (`line()` prefixes the active source), plus
   `test/test_event_log/` (in-memory `Print`, asserting exact output), and migrate
   **PumpControl only** so the API is proven against a node that already has tags,
   hex dumps and change tracking. Additive: every other example keeps building.
2. **Migrate the five copies**: PressureControlNode, PressureControlNode2, WaterLevel,
   SoilNode, and PumpControl's remaining helper. Delete each local `printHexFrame`.
   Add the `EVENT_LOG_HEX` compile flag (default 1) and set it to 0 in the deployment
   envs per the decision above.
3. **Add `lib/LogSink`**: move `LogTee`/`WirelessConsole` out of
   `examples/PumpControl/include/`, fold the auth gate and the verbosity flag behind
   one object, and point PumpControl at it. Keep `PrintController`'s signature intact
   so no `lib/` consumer is touched — riskiest commit, and the one that can be
   dropped without affecting 1, 2 and 4.
4. **MainValve**: adopt both libraries — tags per the vocabulary table above, the
   command-source tags on its serial and LoRaWAN command lines, the payload hex it
   still lacks, and its pressure change line moved onto `Field<T>`. Update
   `README.md:133-138` (diagnostics convention), `AGENTS.md:48-50`,
   `docs/SERIAL_COMMANDS.md`, and this document to the final names. Add the rule to
   `AGENTS.md` that every device tags command-driven lines with the command source,
   so future examples inherit it rather than rediscovering it.

Each commit is independently buildable and testable: `pio run -e <env>` per example,
`pio test -e protocol_compile_tests` for the suites, plus the dashboard JS tests when
the pump or the integrated flow is touched.

Remaining thing to confirm before commit 1: the MainValve tag names, since that table
is a proposal.

## Planned: `[SUBSYSTEM]` tags on MainValve (next session, with the library)

Requested 2026-10-07. MainValve is the one example with no tags at all — it prints
plain sentences through raw `Serial` (= `serialAccess`), for example
`Actuator: actual 45.1 deg (50.1%), target 90.0 deg (100.0%), mode=RS485 bus, ...`
and `LoRaWAN status uplink sent.` (`examples/MainValve/src/main.cpp:1023`,
`:1108-1130`). It also never adopted `PrintController`, because no library it
includes requires one — its Modbus, actuator and command handling are local.

The work: adopt the shared library, give every line a tag, and add the payload hex
that is still missing there (`Planned: LoRaWAN payload hex on MainValve` above). Tag
vocabulary to confirm first, from the existing prefixes in use: `[ACTUATOR]`,
`[PRESSURE]`, `[LORAWAN]`, `[CMD]`, `[MODBUS]`, `[SYSTEM]`. Note the pressure lines
are already change-gated on `lastPressureReadStatus` (`:665-674`), so that is the
first caller to move onto the shared tracker.

## Plan: one vocabulary pass (abbreviated tags), then the libraries

Requested 2026-10-07. One commit that defines the abbreviation table, renames every
tag in every example and library to match, and updates every document that quotes a
tag — so that at any point in history exactly one vocabulary is in force and the docs
always match the code.

Why it must be one commit rather than shortened as files are touched:

- Tags are an interface with the person reading a capture. Two nodes built a week
  apart must not log `[LORAWAN]` and `[LOR]`, or no single grep spans a multi-node
  capture.
- Tags are quoted in `README.md`, `AGENTS.md`, `docs/SERIAL_COMMANDS.md`, this plan and
  the ChirpStack/dashboard notes; a split change leaves a commit where the docs
  describe strings the firmware no longer prints.
- One revert if the abbreviations read badly in the field.

Contents:

1. **The table, approved first.** Live tags: `[SYSTEM]`, `[BOARD]`, `[RS485]`,
   `[VFD]`, `[MOTOR]`, `[PUMP]`, `[CMD]`, `[DEBUG]`, `[MODBUS]`, `[CONTROL]`,
   `[SERIAL]`, `[WIRELESS]`, `[LORAWAN]`, `[POWER]`, `[BATTERY]`, `[FLOW]`, `[SOIL]`,
   `[INVT]`, plus the new `[LOCAL]` and the planned `[ACTUATOR]`, `[PRESSURE]`.
   **Authoritative inventory** (2026-10-07, `grep -rhoE '\[[A-Z][A-Z0-9 ]{1,15}\]'`
   over `examples/`, `lib/`, `include/` — 13 files, ~350 occurrences):

   Slot 1, the subsystem, with occurrence counts: `LORAWAN` 124, `PUMP` 34, `VFD` 27,
   `FLOW` 26, `SYSTEM` 25, `CMD` 17, `MODBUS` 15, `POWER` 10, `CONTROL` 9,
   `BATTERY` 9, `INVT` 8, `SOIL` 6, `DEBUG` 6, `PCV` 5, `PRESSURE` 3, `MOTOR` 3,
   `SERIAL` 2, `RS485` 1, `BOARD` 1.

   Slot 2, the modifiers actually in use: `ERROR` (59 pairs), `TOTAL` (10, under
   `FLOW`), `PROBE` (10, under `FLOW`), `WARN` (3).

   Proposed table — three characters, uppercase, so log columns align:

   ```
   slot 1 (subsystem)              slot 2 (modifier / level)   sources (planned)
   LORAWAN  → LOR   MODBUS  → MOD   ERROR    → ERR             SERIAL   → SER
   PUMP     → PMP   POWER   → PWR   WARN     → WRN             WIRELESS → WLS
   VFD      → VFD   CONTROL → CTL   TOTAL    → TOT             LORAWAN  → LOR
   FLOW     → FLW   INVT    → INV   PROBE    → PRB             LOCAL    → LOC
   SYSTEM   → SYS   PCV     → PCV
   CMD      → CMD   PRESSURE→ PRS
   BATTERY  → BAT   MOTOR   → MTR
   SOIL     → SOI   SERIAL  → SER
   DEBUG    → DBG   RS485   → 485
   BOARD    → BRD   ACTUATOR→ ACT (planned, with MainValve's tags)
   ```

   Uniqueness checked within each slot. `VFD`, `CMD` and `PCV` are already three
   characters. `SER` and `LOR` also appear as source values; that is safe because a
   source only ever follows a subsystem tag, so slot 1 is never a source and position
   disambiguates. Designing the table as a set is what avoids the collisions the
   obvious choices create — `[LORAWAN]`/`[LOCAL]`, `[SERIAL]`/`[SOIL]`/`[SYSTEM]`,
   `[PUMP]`/`[POWER]`/`[PRESSURE]`.

   Byte effect: ~5 characters per tag, so a status line falls from ~91 to ~86
   characters and `.rodata` loses roughly half a kilobyte once identical strings merge.
   Legibility and line width are the reason, not Flash.
2. **`[SERIAL AUTH]` and `[WIRELESS AUTH]` stay long.** They are interactive prompts a
   person reads and answers, not event lines. This also avoids the trap that
   `[SERIAL AUTH]` contains `[SERIAL]`, so replacements must run longest-first.
3. **Docs in the same commit**: `README.md`, `AGENTS.md`, `docs/SERIAL_COMMANDS.md`,
   this document, and any ChirpStack/dashboard note quoting a tag.
4. **Verification**: `pio run` for every env, `pio test`, the dashboard JS suites, then
   a grep proving no long form survives outside the two AUTH prompts.

### Procedure

1. **Approve the table above.** Nothing else starts until the 24 strings are fixed.
2. **Rename longest-first.** `[SERIAL AUTH]` contains `[SERIAL]`, and
   `[WIRELESS AUTH]` contains nothing that collides, so order the replacements
   `[SERIAL AUTH]` before `[SERIAL]`. The two AUTH prompts are exempt anyway; the
   ordering rule exists so a future pass does not corrupt them.
3. **Replace inside string literals only** — the tags appear as `"[LORAWAN] …"` in
   `printf`/`print`/`println` arguments, including the split multi-line forms such as
   `Serial.printf(\n      "[LORAWAN][ERROR] …", …)`. A blind project-wide replace would
   also hit comments and documentation prose; those are updated deliberately in step 5.
4. **Files to touch** (13, all sources that carry tags):
   `examples/PressureControlNode/src/main.cpp`,
   `examples/PressureControlNode2/src/main.cpp`,
   `examples/PumpControl/src/main.cpp`,
   `examples/PumpControl/include/BluetoothConsole.h`,
   `examples/PumpInvtGD200ANode/src/main.cpp`,
   `examples/SoilNode/src/main.cpp`,
   `examples/WaterLevel/src/main.cpp`,
   `lib/CommandProcessor/src/CommandProcessor.cpp`,
   `lib/DelixiCDIE100/src/DelixiCDIE100.cpp`,
   `lib/PressureNodeCommandProcessor/src/PressureNodeCommandProcessor.cpp`,
   `lib/PumpController/src/PumpController.cpp`,
   `lib/Rs485ModBus/src/RS485ModBus.cpp`,
   `lib/SerialAccess/src/SerialAccess.cpp` (AUTH prompts only — no change).
   MainValve has no tags yet, so it is untouched here; its tags arrive with commit 4
   already abbreviated.
5. **Docs in the same commit**: `AGENTS.md` (add the table as the rule), `README.md`,
   `docs/SERIAL_COMMANDS.md`, each example `README.md`/`CHANGELOG.md` that quotes a
   log line, `examples/PumpControl/README.md` (the serial event-log section I added),
   and this document — including its own "Conventions to keep" list and the tag
   mentions in the planned sections.
6. **Update the `[SUBSYSTEM]` list in this document** to the new short set, so the
   conventions section never describes tags the firmware stopped printing.

### Verification

- `pio run -e pump_control -e main_valve -e pcv_low_power_class_a
  -e pcv_low_power_class_a_without_flowmeter -e water_level -e soil_node
  -e invt_gd200a_staging` (and `-e pcv_hybrid_class_c` if it is kept), each must build.
- `pio test -e protocol_compile_tests` and `-e pressure_logic_compile_tests`.
- `node tools/test_pump_control_dashboard.js`, `test_irrigation_dashboard.js`,
  `test_flow_ids.js` — the flows assert codec field names, not tags, so they should
  pass untouched; a failure means a tag was renamed inside a protocol string by mistake.
- Inventory re-run: the same `grep` must return only the 24 short strings plus
  `[SERIAL AUTH]` / `[WIRELESS AUTH]`. Any long form still present is a missed site.
- Spot-check one live console from any node for a `[LOR]`/`[PMP]`/`[FLW]` line.

### Non-goals

- Not renaming log *wording*, node names, command tokens (`CONFIRM`, `CONFIRM_WRITE`),
  codec field names, or the `meaning [code]` formatters.
- Not renaming anything in the Node-RED flows or the ChirpStack codecs.
- Not touching MainValve here (commit 4 gives it tags already abbreviated).

### Sequencing

This lands **before** the `EventLog` work, as commit 0. Once the library
exists, a tag is a value (`Tag::LoRaWan`) whose abbreviation lives in one table inside
the library, so no call site carries a tag literal and any future re-vocabulary is a
one-line edit. Abbreviating after the library lands would rename every example, its
tests and its docs twice.

## Status of the questions raised

All four answered on 2026-10-07:

1. Payload hex — **decided: switchable, compile-time.**
2. MainValve tags — **proposed:** the six-tag table above, applied with the library in
   commit 4. Confirm the names before commit 1.
3. Valve-node events — **proposed:** the discrete list above (commanded state,
   `status_reason`, sensor-validity transitions, battery validity/threshold, flow-total
   validity on valve_1). Confirm before writing it.
4. Shared tracker — **recommended:** per-field `Field<T>`, not a whole-struct compare,
   for the padding, float-drift and "something changed" reasons above.

One thing left for the user to confirm before commit 1: the MainValve tag names.
5. Command source on every command-driven line — **required by the user on
   2026-10-07 for all devices**, design settled in its own section above: per-command
   attribution (not one global latch), source beside the subsystem tag, replies fan
   out to every channel, tag `[WIRELESS]`.

## Verification for either node

- `pio run -e water_level` / `pio run -e soil_node` (PlatformIO lives at
  `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`).
- Open the monitor with the password inside its 30-second window, capture one wake
  cycle, and confirm each change prints exactly once and a steady state prints
  nothing.
- Confirm the `meaning [code]` form on a deliberately induced failure where the
  hardware allows it.
