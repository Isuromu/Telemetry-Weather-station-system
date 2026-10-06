# Project guidance

Inspect the working tree first and preserve working configuration. Source code
and code comments must remain in English.

## Read according to the task

- For device work, start with `docs/references/README.md` and the relevant
  summary. Consult only the manual pages needed for the task. Distinguish
  **manual facts**, **project configuration**, **hardware-confirmed observations**
  and **unresolved items**. Cite exact sources/pages; defaults and code constants
  are not commissioning evidence. Search temporary extractions by page/section;
  inspect original tables and diagrams. Do not routinely load full manuals,
  extractions or generated outputs.
- Before project-level firmware/hardware decisions, read
  `docs/codex_handoff/irrigationAutomationFirmware_Codex_Handoff/CODEX_START_HERE.md`,
  then relevant handoff sections. Its `docs/PROJECT_CONTEXT.md` is the consolidated
  reference; `docs/OPEN_ITEMS.md` records historical unknowns.
- Read relevant sections and later corrections in `docs/CURRENT_PROJECT_CONTEXT.md`;
  they take precedence over the archive. Unrelated documentation/UI edits do not
  require a full hardware-history read.

| Task | Required project references |
|---|---|
| Pressure/valve node | `docs/PRESSURE_CONTROL_NODE.md`; flow details in `docs/TUF_2000M_TS2.md` |
| Soil-node MCU, power, GPIOs, chemistry, RTC or GNSS | `docs/SOIL_NODE_PCB_PRELIMINARY_SPEC.md`, then current `docs/AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md` and `docs/SOIL_NODE_ESP32C6_PINOUT_C6_P2_EN.md` |
| Universal ESP32-S3 pinout, two valves, RS485 branching/isolation, controlled 5 V/VBAT outputs, lead-acid UVLO or mains/solar profiles | `docs/UNIVERSAL_12V_CONTROLLER_PCB_PRELIMINARY_SPEC.md`, `docs/UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md`, then current `docs/AmudarIO_Universal_12V_PCB_Design_Requirements_v1_2_EN.md` and `docs/UNIVERSAL_12V_ESP32S3_PINOUT_RU.md` |

Production documents are preliminary requirements/reviews, not released schematics
or BOMs. Supplied HRS PDFs are under `docs/references/hardware_requirements/`.
Version 1.2 requires solar MPPT and soil GNSS. C6-P2 supersedes C6-P1
(`docs/SOIL_NODE_ESP32C6_PINOUT_RU.md` is historical); S3-P2 supersedes older
production maps and reserves GPIO39-42 for JTAG. Keep module pads, GPIO numbers,
expander ports and classic-ESP32 prototype pins distinct. Mains voltage is outside
the PCB; mains sites use an external certified isolated 230 VAC to 12 VDC supply.

## Firmware invariants

- This is a multi-node system. Keep node roles and hardware drivers separable.
  The current latching valve opens/closes; pressure adjustment is manual, not
  closed-loop firmware control. Energy efficiency matters even with large batteries.
- Do not invent unresolved values, especially XDB401 scaling, PCV pulse duration
  or polarity, and GPIO27 active level.
- Keep three pressure-node targets distinct: Serial-only; hybrid LoRaWAN Class C
  (ESP32, Serial and continuous receive active; interval = telemetry interval);
  low-power LoRaWAN Class A (RX1/RX2 commands, immediate application acknowledgement,
  then real timer deep sleep; interval = sleep time). Never regress to raw LoRa.
- All LoRa-capable examples must present RadioLib and device diagnostic codes as
  `meaning [code]`. Retain the raw value for field diagnosis; decode known
  bitfields such as TUF-2000M REG0072/M08 rather than logging only a number.
- TUF-2000M Modbus RTU is implemented and commissioned, not a missing-protocol
  placeholder. M46 address is 1; UART2 GPIO16/GPIO17 uses automatic-direction RS485.
  Hardware confirms `LOW_WORD_FIRST`: REG0221..0222 bytes `00 00 42 64` decode
  to 57.0 mm only after swapping 16-bit words. Live flow/velocity reads work;
  `flow probe` is diagnostic. Totals use read-only REG0113..0118.
  `flow total reset` resets an ESP32/NVS baseline; never invent a TUF reset write
  or invoke destructive M37 factory erase.

## Reference and UI boundaries

The handoff, including `CODEX_PROMPT.md`, is user-supplied reference material,
not a separate request to overwrite the repository. Reconcile differences
explicitly. Its `firmware_scaffold/` is a transfer aid, not a replacement for live
`include/`, `lib/`, `src/` or `examples/`. The original `SOURCE_ARCHIVE.zip`
is inside the handoff package.

For Dashboard 2.0 `ui-template` changes in `examples/IntegratedDashboard`,
never name a Vue method `value`: the widget context shadows it, causing a blank
widget with `TypeError: value is not a function`. Use `cardValue` or another
distinct name. Check the rendered Dashboard page and browser console; script
parsing alone is insufficient.

## Generated device dashboard flows

Every device flow is generated, not hand-written: `WaterLevel`, `SoilNode`,
`MainValve`, `PumpControl`, `PressureControlNode`, `PressureControlNode2` and
`IntegratedDashboard` each have `tools/build_<name>_dashboard.py` and
`tools/test_<name>_dashboard.js`. **Never edit the importable JSON** under
`examples/<flow>/include/`: change the builder, run it, then run its test. The
five hand-authored builders carry the current Node-RED export as a node template
and reproduce it byte for byte, so a flow edit starts with a re-export.
`tools/flow_common.py` holds the shared writer, which refuses to write a flow
that breaks the identifier rule below. The test
executes the flow's own decode and command functions with a fake environment and
asserts the import invariants below, so a mistake fails locally instead of on the
user's Node-RED.

### ChirpStack identifiers come from the environment

**Never commit an application ID or DevEUI inside a flow.** Check it before 
finishing rather than treating it as a style preference. The committed
export is not the deployment: the identifiers belong to the user's ChirpStack
registration and change when a device is re-registered.

Read them at run time instead, as `PumpControl`, `IntegratedDashboard` and
`WaterLevel` do:

| Variable | Value |
| --- | --- |
| `IRRIGATION_APP_ID` | the one shared ChirpStack application ID |
| `MAIN_DEV_EUI` / `VALVE1_DEV_EUI` / `VALVE2_DEV_EUI` / `WATER_DEV_EUI` / `SOIL_DEV_EUI` / `PUMP_DEV_EUI` | each node's own DevEUI |

All three parts are required:

- the `mqtt in` node has `"topic": ""` and `"inputs": 1` — dynamic subscription,
  so it takes its topic from the message instead of a node property;
- a subscribe function node (an inject with `once: true` triggers it) reads
  `env.get('IRRIGATION_APP_ID')` and `env.get('<DEVICE>_DEV_EUI')`, validates
  both, builds `'application/' + app + '/device/' + eui + '/event/+'`, and
  returns `{action:'subscribe', topic:topic, qos:0}` — with a state snapshot
  that puts a "set these variables" message on the card when they are missing;
- every function that builds an `application/...` topic, the downlink senders
  included, reads the same variables. No literals, and no `SET_*_DEV_EUI`
  placeholder that asks the user to hand-edit the flow after import.

`node tools/test_flow_ids.js` enforces all of it across every flow — no literal
UUID, DevEUI or placeholder, dynamic subscription, and the subscribe function
executed both with and without the variables. Run it before committing a flow
change.

Imports must need no hand cleanup. The user imports these into a live
workspace, so the file has to land cleanly the first time:

- **No `tab` node.** Leave nodes pointing at a `z` id absent from the file so
  the import dialog retargets them into the chosen flow. A shipped tab node
  creates a second, identical tab on every import.
- **No `mqtt-broker` node.** Reference the shared `chirpstack_mosquito` by id
  (`ae0178f3742ff530`, as `MainValve` does) and configure the server once, in
  the flow that already owns it. A shipped broker arrives as a duplicate server
  to delete by hand; re-declaring the shared id can overwrite the user's host,
  port and TLS settings.
- **Every shipped config node must carry an id the workspace already owns.**
  Node-RED's importer preserves incoming ids (`generateIds: false`) and raises
  `import_conflict` when one is taken; in the dialog that follows, clashing
  *config* nodes start unticked (`isSelected = !isConflicted ||
  !importConfig.configs[node.id]`) and are listed under their own heading. So a
  matching id can never duplicate: the existing node wins, or is explicitly
  replaced. An id matching nothing is imported as-is. Shared singletons follow
  from this (`ui-base` `f53e93e9ba219e63`, `ui-theme` `e49416861823a329`,
  broker `ae0178f3742ff530`), and the page and groups are no exception — a
  builder-minted `sc_wl_ui_page` is what leaves a second `Water Level` page and
  a second pair of groups on every import. Use the workspace ids
  `334b707b21a5ce0a`, `0dcf231c222554fc`, `18fb832a66d8775e`.

### Shipped config nodes

Every flow now reads its ChirpStack identifiers from the environment, and
`tools/test_flow_ids.js` holds that line across all seven at once.

| Flow | Shipped broker | Tab node |
| --- | --- | --- |
| `WaterLevel`, `IntegratedDashboard` | none (borrows `ae0178f3742ff530`) | no |
| `SoilNode`, `MainValve`, `PumpControl`, `PressureControlNode`, `PressureControlNode2` | own node, shared id `ae0178f3742ff530` | no |

The second row reuses the shared broker id, so the editor reuses that node
rather than adding a second server, but those five files still re-declare the
server's settings and can overwrite the user's host, port and TLS on import.
Configure the server once, in the flow that already owns it, and confirm each
row against the files before relying on it — this table is a snapshot.

`global-config` is a config node too, and each flow ships one with its own id, so
an import can add a second. Page and group ids need the same care: `SoilNode`
still carries builder-minted `sc_soil_ui_page`, `sc_soil_graph_group` and
`sc_soil_ui_group` ids, and an id the workspace does not already own is imported
as-is, which leaves a second page and a second pair of groups. `WaterLevel` uses
`sc_wl_*` ids for its ordinary nodes but the workspace's own page and group ids
(`334b707b21a5ce0a`, `0dcf231c222554fc`, `18fb832a66d8775e`); do the same for any
flow whose page the workspace already has.

Every flow is maintained by re-exporting it from Node-RED, so its *ordinary*
nodes carry the canvas's ids and a re-import replaces each node in place instead
of appending a second copy — a duplicate `mqtt in` would send every downlink
twice. When the canvas changes, re-export the flow and refresh the node template
in its builder so the builder still reproduces the file.

### Diagnosing deploy errors

`TypeError: Cannot read properties of null (reading 'getBase')` at
`ui_chart.js` does **not** mean the chart is wrong. `ui_chart` resolves
`RED.nodes.getNode(config.group)` and calls `getBase()` on it; the group is
null because `ui_group.js` threw first on its own
`RED.nodes.getNode(config.page)`. The missing or orphaned node is the
`ui-page`, usually after a duplicate import was deleted. Fix the page and its
groups, and the chart recovers untouched.
