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

Device flows (`WaterLevel`, `SoilNode`, `PressureControlNode*`) are generated,
not hand-written. Never edit the importable JSON: change
`tools/build_<name>_dashboard.py`, then run the builder **and**
`tools/test_<name>_dashboard.js`. The check re-runs the decoder against a
recorded uplink and asserts the import invariants below, so a mistake fails
there instead of on the user's Node-RED.

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

### Not yet brought in line

`WaterLevel` and `IntegratedDashboard` are the only flows that follow all of the
above. The rest are hand-authored JSON, and only those two have a builder at
all, so the others cannot simply be regenerated — fixing one means editing its
JSON by hand, or introducing a builder for it.

| Flow | Shipped broker | Tab node | Builder |
| --- | --- | --- | --- |
| `WaterLevel` | none (borrows `ae0178f3742ff530`) | no | `build_water_level_dashboard.py` |
| `SoilNode` | **own copy, id `sc_ae0178f3742ff530`** | no | none |
| `MainValve`, `PumpControl`, `PressureControlNode`, `PressureControlNode2` | own node, shared id `ae0178f3742ff530` | no | none |
| `IntegratedDashboard` | none (borrows `ae0178f3742ff530`) | no | `build_irrigation_dashboard.py` |

`SoilNode` is the one with a live defect: its broker id is distinct from the
shared one, so importing it adds a second `chirpstack_mosquito` server to
delete by hand — exactly the chore `WaterLevel` no longer causes. The middle
row reuses the shared id, so the editor reuses that node rather than
duplicating it, but those files still re-declare the server settings and so can
overwrite the user's host, port and TLS on import. Confirm each row against the
files before relying on it; this table is a snapshot, not a guarantee.

`IntegratedDashboard` follows the same rules and adds one the others do not
need: its *ordinary* nodes carry the canvas's ids too (`fd327e43632c19d6`,
`0da71259acd21508`, `ed30a564d5bb461a`, …), because that flow is maintained by
re-exporting it from Node-RED. A re-import then replaces each node in place
instead of appending a second copy — a duplicate `mqtt in` would send every
downlink twice. Re-export rather than hand-editing, and refresh the ids in
`build_irrigation_dashboard.py` if the canvas ever changes.

### Diagnosing deploy errors

`TypeError: Cannot read properties of null (reading 'getBase')` at
`ui_chart.js` does **not** mean the chart is wrong. `ui_chart` resolves
`RED.nodes.getNode(config.group)` and calls `getBase()` on it; the group is
null because `ui_group.js` threw first on its own
`RED.nodes.getNode(config.page)`. The missing or orphaned node is the
`ui-page`, usually after a duplicate import was deleted. Fix the page and its
groups, and the chart recovers untouched.
