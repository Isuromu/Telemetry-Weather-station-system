# Wireless debug logging plan

Goal: every node's event log readable **on the laptop over WiFi**, so a live failure can
be read from the device that experienced it and compared line by line against what the
dashboard believed. One 2.4 GHz router, all six nodes join it and stream, and an agent
tails the merged stream for real-time bug hunting.

This plan does **not** replace the logging work already planned in
`docs/SERIAL_EVENT_LOGGING.md`. That plan splits logging in two — **`EventLog` = what the
line says**, **`LogSink` = where the bytes go** — and this plan adds one more `LogSink`
implementation plus the host collector. Vocabulary, tags, `hex()` and change tracking stay
where that document puts them.

Why it matters for the current bugs: the dashboard cannot distinguish "the device never
received the close" from "it answered and the uplink was lost". The device's own log can.
That is the acceptance test for the dashboard rewrite in steps 2–3 of the main plan.

## Decisions taken 2026-10-08 (user)

| # | Decision |
| --- | --- |
| 1 | **One 2.4 GHz router**, every node joins it and streams logs. WiFi becomes the channel the operator *reads*; USB and the pump's Bluetooth stay compiled in but stop being the working channel. |
| 2 | **Everything the USB channel prints is sent over WiFi** — no filtering, so a WiFi line and a USB line are byte-identical and comparable. |
| 3 | **One compile flag, no new PlatformIO environments.** `-D WIFI_LOG=1` turns the channel on; `WIFI_LOG` defaults to 0 so a production build cannot grow a WiFi radio by accident. |
| 4 | **Three separate pieces of work, not one series**: `EventLog` + `LogSink` (printing becomes general), then WiFi serial (the sink + collector), then adoption by all devices. Device integration stays out of git while it is being tested. |
| 5 | The pump keeps its Bluetooth console (§5, answer 5). |
| 6 | Logs must be usable by an agent in **real time**, so the collector must expose one merged, tail-able stream — not only per-node files. |

## What exists today

| Piece | State |
| --- | --- |
| USB serial | every example; `monitor_filters = time` stamps lines on the host |
| Bluetooth Classic SPP | PumpControl only, `PUMP_BT_CONSOLE=1`, `LogTee` (USB + BT), `WirelessConsole` |
| `LogTee` | fans a line to two sinks; the second sink receives **whole lines**, because per-byte writes flood a slow channel's TX queue and stall the control loop |
| Password gate | `lib/SerialAccess` + `config/SerialAuthSecrets.h` on the USB stream; the BT console reuses it for commands |
| MQTT tools | `irrtest/correlate.py` (downlink id → device echo), `irrtest/run_click_cycle.py`, `irrtest/cdp.py` |
| WiFi | **nothing yet.** No example includes `WiFi.h` |

`LogTee`/`WirelessConsole` is the precedent to copy: a second channel that gates itself,
buffers whole lines, and can be compiled out.

## Confirmed: the WiFi channel carries logs only

The WiFi sink is a `Print` — it only receives bytes other code already printed, and has no
path to the command handlers. USB/BT command input (password-gated) is untouched by it.
The user has confirmed this is the intent: **WiFi serial is not a command processor**, so
there is no WiFi command path to build, and no way for this work to change how a node
accepts commands.

## The LoRaWAN coexistence risk, in detail

Only **MainValve** is materially at risk, because it is **Class C**: its receiver is open
continuously, so a downlink can arrive at any instant and the node must already be
listening. Everything else is Class A — it opens RX1/RX2 in a known, short window after
its own uplink, so the only question there is whether the window is served on time.

Three mechanisms, in order of how likely they are to matter:

1. **CPU timing, not radio physics — the real one.** `radio.receive()`/`sendReceive()` is
   a cooperative, blocking call: the SX1262 buffers a packet in FIFO and raises DIO1, but
   the firmware has to notice and read it out. Wi-Fi brings non-preemptible work into that
   loop: `WiFi.begin()` channel-scan (seconds), DHCP, and lwIP socket writes. If such work
   is running in the same task when a downlink lands, the packet waits in the radio's
   256-byte FIFO. One packet fits and survives; a second one arriving before the first is
   read overwrites it. So the failure mode is **a missed downlink under a burst of log
   traffic**, which looks exactly like the bug we are chasing (dashboard: "sent"; device:
   nothing).
2. **Supply current.** Wi-Fi transmit bursts draw several hundred milliamps from the same
   3.3 V rail as the SX1262. A sag or a brown-out dip degrades receiver sensitivity, and a
   brown-out resets the ESP32 (which would also lose the RTC LoRaWAN session and force a
   rejoin — visible in the log as a join, so it is diagnosable, but it is a real hazard on
   a long thin supply).
3. **RF coupling.** 2.4 GHz and 868 MHz are far apart, and the 868 MHz band is not a
   harmonic of 2.4 GHz, so in-band interference is not the mechanism. Broadband noise from
   the Wi-Fi PA and the antenna's near field can raise the SX1262 noise floor; with a
   gateway a few metres away that is a link-margin question, not a link-loss one.

Mitigations that follow from the mechanisms, not from habit:

- **No channel scan, no rejoin loops.** Join by SSID+BSSID once, keep the connection
  alive, and only reconnect when the socket dies.
- **Drain on the other core with a bounded chunk.** On dual-core ESP32 (MainValve, valves,
  pump, WaterLevel) pin Wi-Fi/lwIP to one core and the radio loop to the other, and let the
  drain send a bounded number of lines per pass so it always yields. `write()` still only
  appends to the ring, so the caller never waits.
- **Throttle by need.** The sink is event-oriented: with full pass-through of everything
  the USB channel prints, the quiet node costs almost nothing, and a chatty node is
  bounded by a lines-per-second cap (extra lines are dropped and counted, never queued
  without limit).
- **Measure it, per node, before trusting it.** For each node: run 10 minutes with the
  sink off, then 10 minutes with it on, and compare the counts the node itself prints —
  `[LORAWAN]` uplinks sent, uplinks failed, downlinks received, join attempts, and any MIC
  mismatch. Then compare the device's downlink count against what the dashboard published.
  Only the downlink *count* needs to hold. A drop means the drain or the supply is the
  problem, and the answer is a throttle, not a redesign.
- **Fallback is per node, not global.** MainValve is the node whose logs matter most and
  the one with the risk. If its downlink count degrades with Wi-Fi on, that node keeps
  USB/BT for the session (it is a mains node on the bench) while the other five run Wi-Fi,
  and its section of the plan is revisited with the measurement in hand.

### Realisation on the Class C nodes: MainValve **and** PumpControl

Both are Class C and both poll the radio once per loop iteration —
`lorawan.getDownlinkClassC(...)`, then the uplink scheduler, then `delay(5)`
(`examples/MainValve/src/main.cpp:1275`, `examples/PumpControl/src/main.cpp:687-741`).
Reception therefore depends on each loop iterating at roughly that cadence, and the only
thing Wi-Fi may change about those loops is: nothing. Same three tiers, cheapest first.

**The pump carries an extra risk that already exists: its Bluetooth console writes from
the loop task.** `LogTee logTee(serialAccess, wirelessConsole)` with `#define Serial
logTee` means every printed line goes to USB *and* to the SPP socket from the same task
that polls the radio, and the header says it plainly — a full 32-slot BT TX queue "can
stall the control loop for up to `SPP_TX_QUEUE_TIMEOUT`"
(`examples/PumpControl/include/BluetoothConsole.h:30`). That is the exact mechanism this
plan is trying to avoid, and it is live today. Two consequences:

- The WiFi sink must not copy that design: ring-only in the caller, drain on the other
  core. Done right, **WiFi is safer for the pump's radio than the BT channel it already
  has.**
- **Do not stream both 2.4 GHz channels at once.** BT Classic and Wi-Fi share the same
  radio through the coexistence arbiter, so running the SPP console *and* the log stream
  splits the time between them and adds jitter to both. For a WiFi debug session build the
  pump with the flag the header already supports — `-D PUMP_BT_CONSOLE=0` (which also
  frees ~776 KB of flash, per `examples/PumpControl/CHANGELOG.md`). The BT code stays in
  the tree and in production builds; only the debug build compiles it out. In the field
  the pair is reversed: `PUMP_BT_CONSOLE=1`, `WIFI_LOG=0`.

The pump's VFD/RS485 polling is time-critical for the same reason its USB link is
unreliable under motor noise, so the `radioBusy` gate in tier 2 covers the Modbus
transaction as well as the LoRaWAN poll — one flag marking "this task is talking to
hardware", read by the drain.

**Tier 1 — default.** The sink's `write()` only appends to a 2 KB ring
(`portENTER_CRITICAL`, no allocation, no network), so the radio task never blocks. A
separate task owns the socket, **pinned to the other core** (the node is an
ESP32-WROOM-32D, dual core; Arduino's `loopTask` runs on core 1, Wi-Fi/lwIP on core 0):

```cpp
void WifiLogSink::write(uint8_t b) {          // radio task, must stay ~microseconds
  const size_t next = (head_ + 1) % kRingSize;
  if (next == tail_) { ++dropped_; return; }  // full: drop and count, never block
  ring_[head_] = b; head_ = next;
}
// created only when -D WIFI_LOG=1, core 0, priority 1, 4 KB stack, no dynamic alloc
void WifiLogSink::task() {
  for (;;) {
    if (radioBusy_) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }        // tier 2 gate
    if (!linkUp()) { joinOnce(); vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    const size_t n = drainUpTo(kMaxLinesPerPass, kMaxBytesPerPass);    // coalesced write
    if (n == 0) vTaskDelay(pdMS_TO_TICKS(20)); else taskYIELD();
  }
}
```

Join once by **SSID + password + channel + BSSID** (`WiFi.begin(ssid, pass, ch, bssid)`),
so there is no channel scan; keep the link up; `WiFi.setSleep(true)` for modem sleep —
fewer RX interrupts and lower average current, paying only TX latency, which a log
channel does not care about. In the drain: coalesce up to a few lines or ~512 B per TCP
write, cap the rate (excess is dropped and counted), truncate a single line at 200 B and
mark it, and **never print from the drain task** — the USB channel has one writer (the
caller's task), and the drain's own health goes into the TCP stream as `[WIFI]` lines.

**Tier 2 — if the measurement shows a downlink drop.** Gate the drain on the radio: set a
`volatile bool radioBusy_` around `getDownlinkClassC(...)`, `sendStatusUplink(...)` and the
I²C reads, and have the drain skip a pass while it is set. The flag is a single
load/store, so the radio side pays nothing, and a socket write can no longer overlap a
radio transaction. Combined with a 50 ms drain tick, all six nodes can use it; it costs
latency in the log channel only.

**Tier 3 — if it still degrades.** Leave `WIFI_LOG` off for that node and read it over
USB/BT. The sink, the collector and the other five nodes are unaffected, so this costs one
flag, not a redesign.

**What is measured, and by what.** The node's own `[LORAWAN]` lines are the instrument:
downlinks received, uplinks sent, uplinks failed, join attempts, MIC-mismatch recoveries.
Baseline 10 minutes with the flag off, then 10 minutes with it on, and a third run with a
synthetic log flood (the sink at full rate) to stress it deliberately. Acceptance: the
downlink count matches what the dashboard published in all three runs. The sink's own
`[WIFI]` lines (up, RSSI, reconnects, dropped) explain any difference.

For the pump the same three runs apply, plus one reading that only it has: the VFD
Modbus error count (`[VFD]`/`[MODBUS]` lines). The BT console's blocking writes and the
WiFi drain are two versions of the same hazard, so the comparison that matters is
BT-with-WiFi-off versus WiFi-with-BT-off — not both.

## Design

### 1. The sink — `lib/LogSink/` gains a third implementation

One library, three implementations, one name each; the sink lands **inside** `lib/LogSink`
rather than as a new library:

- **`write()` never blocks and never touches the network.** It appends to a fixed ring
  buffer (2–4 KB, RAM) and returns. The ring full ⇒ drop the line, count it, print one
  `[WIFI][WARN] dropped N lines` record when space returns. The BT sink today can stall
  the control loop for `SPP_TX_QUEUE_TIMEOUT` when its TX queue fills; this one must not.
- A low-priority drain task owns the socket: reconnect with backoff (1 s → 30 s). The
  drain is chunked and yields, because MainValve's radio receive must always win.
- **Full pass-through** (decision 2): whatever the USB channel gets, WiFi gets, so the two
  streams are comparable without a second set of rules.
- **No timestamps in firmware** (`docs/SERIAL_EVENT_LOGGING.md`: the host owns the clock).
  The node sends its identity plus the line; the collector stamps arrival.
- Node identity comes from the same flag: `-D WIFI_LOG_NODE="mainvalve"`. No new envs.

Testability, which is why this is a library and not example code: the sink takes a
`Transport&` (open/write/close) instead of using `WiFiClient` directly, so
`test/test_log_sink/` drives it with a **fake transport** — asserting line framing, that a
full ring drops and counts rather than blocking, that a dead transport retries with
backoff and loses nothing already buffered, and that a `write()` call stays under a
bounded time with the transport stalled. No radio, no network, no hardware in the test.

### 2. Transport and the collector

**TCP, device as client**: each node connects out to the laptop, so nothing listens on an
ESP32, no inbound rule is needed, and reconnect is just a retry. UDP would be simpler and
lossy — the wrong trade for evidence the operator has to trust.

`tools/wireless_log_collector.py` (stdlib only, Python 3.8 per this repo's rule):

- One TCP connection per node; **per-node JSONL** (`irrtest/wireless/<node>-<HHMM>.jsonl`)
  and **one merged stream an agent can tail** — decision 6. Merged line shape:
  `{"t":"19:24:07.412","node":"mainvalve","src":"device","line":"[LORAWAN] Downlink: 01 01 00 00 00 70"}`
  with `src` distinguishing device lines from MQTT lines.
- **Also subscribes to the ChirpStack MQTT topics** (host `192.168.2.217:1883`,
  anonymous — verified) and writes them into the same stream, so one file holds: what the
  dashboard published, what the device logged, and what the device uplinked. That is what
  makes "compare with the device debug logs" mechanical instead of manual.
- Prints a live tail, one line per event, **prefixed with the node name host-side**
  (`19:24:07 mainvalve [LORAWAN] Downlink: …`). The node prefix is added by the collector,
  not by the firmware, so the device's line stays byte-identical to its USB line and the
  two streams can be diffed. This is the "separate by log tag" the user asked for, without
  putting a second tag into the vocabulary.
- `--no-mqtt` for a device-only capture.

**Why JSONL rather than plain text or CSV.** The collector merges two differently-shaped
sources (device lines and MQTT messages) into one growing file that a person tails *and*
an agent parses, so the format has to be line-framed, fielded, and opaque-safe:

- **A newline is the frame boundary.** A TCP stream delivers arbitrary chunks; with one
  JSON object per line, a partial record is the only casualty of a kill, and the reader
  needs no length prefix or parser state. The repo already works this way:
  `irrtest/timeline_run*.jsonl` and `cycles_run*.json` come from the same idea.
- **Fields without a schema fight.** `{"t","node","src","line"}` lets the merged file hold
  device lines and MQTT lines together, and lets a later reader add a field
  (`id`, `fCnt`, `phase`) without breaking what already reads the file.
- **The device's own text stays opaque.** The log line goes in a JSON string, escaped by
  the encoder. CSV would need quoting around commas, quotes and control characters in
  lines the firmware already produces — escaping bugs in the one artefact meant to be the
  evidence.
- **It stays shell-friendly.** `tail -f`, `grep '"node":"mainvalve"'`, `jq -r .line`,
  `wc -l` all work; nothing about JSONL forces a reader to use a JSON parser, and nothing
  about plain text gives you a per-record source field.

Cost, for completeness: about 45 bytes of envelope per line, and the raw byte stream is
not human-readable without `jq`/`grep` — which is why the collector *also* prints the live
tail as plain text with a host-side node prefix.

### 3. Enabling it without new environments (decision 3)

`lib/LogSink` ships `WIFI_LOG` defaulting to 0. To build one node with the channel on,
pass the flag; nothing in `platformio.ini` needs a new `[env:]`:

```bash
PLATFORMIO_BUILD_FLAGS='-D WIFI_LOG=1 -D WIFI_LOG_NODE="mainvalve"' \
  pio run -e main_valve -t upload
```

Secrets live in `config/WirelessSecrets.h` (ignored) with a `.example` template:
`WIFI_SSID`, `WIFI_PASSWORD`, `COLLECTOR_HOST`, `COLLECTOR_PORT` — the same pattern as
`config/SerialAuthSecrets.h`. If a session wants the flag pinned instead of typed, it goes
in the shared `build_flags` block temporarily and comes out before the next commit.

### 4. Deep-sleep nodes (valve_1, valve_2, WaterLevel, SoilNode)

A join cannot survive deep sleep. Per wake: join (bounded, e.g. 3 s) → flush the ring →
sleep. Lines produced before the join are buffered in RAM and flushed at wake end;
anything still unflushed is counted in the next line that does go out. Bench builds
already use 10 s intervals (`PCV_LORAWAN_DEFAULT_INTERVAL_SECONDS=10`,
`WATER_LEVEL_SLEEP_SECONDS`), so a bench session sees every wake. RTC-retained buffering
across sleep is possible and deliberately **not** planned: it costs RAM the nodes need,
and the bench does not need it.

## Sequencing: three separate pieces of work (decision 4, user's grouping)

Not one commit series — three, each useful on its own, in this order:

### A. `EventLog` + `LogSink` — printing becomes general

The libraries that make printing the same everywhere and simple to add to any project:
`EventLog` = what the line says (`line()`, `hex()`, the `Field<T>` change tracker, the
`meaning [code]` formatters), `LogSink` = where the bytes go (the single `Print` a node
passes around, absorbing the USB auth gate, the verbosity flag and the USB/BT fan-out).
This is the five-commit plan already written in `docs/SERIAL_EVENT_LOGGING.md` (vocabulary
pass → `EventLog` → `LogSink` → migrate the examples); nothing in this document changes
it. **Keep both libraries project-general**: no irrigation tags, no node names, no
irrigation-specific field lists inside them — a tag vocabulary and a watched-field list
belong to the project that uses them, or the second project to adopt the libraries has to
fork them.

### B. WiFi serial — one more `LogSink` implementation, for bug hunting now

`lib/LogSink` gains the WiFi sink (`Transport&` interface, ring buffer, drop-and-count,
throttled low-priority drain, `-D WIFI_LOG=1` / `-D WIFI_LOG_NODE="..."`),
`config/WirelessSecrets.example.h`, `test/test_log_sink/` driven by a fake transport, and
`tools/wireless_log_collector.py` with its `--selftest`. Device code is untouched in this
piece: every example still builds and behaves exactly as before, flag off by default.

### C. Adoption

All six nodes use the sink, still flag-gated, with their per-node wiring and their
`[LORAWAN]`-count verification in the tree. The pump's Bluetooth console stays (decision
5); USB stays as the fallback channel everywhere.

**Between B and C: local testing that does not go to git** (the user's rule). Wire the sink
into one node — MainValve first, since its failures are the reason for all of this — build
with the flag, and run the before/after `[LORAWAN]` count comparison from the coexistence
section. These per-device edits are scaffolding: they are tested on the bench and stay out
of history until piece C commits the adoption properly.

## How this feeds steps 2–3 of the main plan

The dashboard rewrite and the device logs are one loop: the dashboard's checks should be
claims a device log can confirm or refute. Every decision that today rests on an
*inference* ("silence means the downlink was lost") becomes checkable against the merged
stream — dashboard published close id N → device `Downlink: … id N` → device `Target set:`
→ uplink `reported_command_id=N`. A missing link names the failing layer.

The evidence/policy split (evidence = what device logs settle, policy = the operator's)
stays a later step, to be written down and committed on its own.

## Non-goals

- **No command input over WiFi.** No remote shell, no OTA, no file transfer. The
  USB/BT password gate stays the only command path; the log channel cannot become one.
- No change to LoRaWAN payloads, codecs, or the MQTT topic scheme.
- No persistent on-device log storage: the collector's files are the record.
- No new PlatformIO environments.
