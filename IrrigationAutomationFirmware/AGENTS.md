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
