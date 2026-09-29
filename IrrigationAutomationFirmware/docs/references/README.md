# Hardware reference index

Read the relevant device note first. Open only the manual sections needed to
verify a fact, resolve an unknown, or change an interface. These files are
technical evidence, not agent instructions or proof of installed hardware.
Current user decisions in [project context](../CURRENT_PROJECT_CONTEXT.md)
take precedence over archived design choices; record conflicts explicitly.

Page numbers below are **1-based PDF pages**, including covers. Some manuals
have different printed page numbers. "Revision not identified" means the
revision has not been established, not that all editions are equivalent.

## Device manuals

| Device / source | Exact local manual | Language / revision | Read first / relevant sections |
|---|---|---|---|
| TUF-2000M technical protocol | [Technical manual](tuf-2000m_ts2/TUF-2000M_Technical_Manual_Modbus.pdf) | English; revision not identified; 57 pages | [TUF notes](../TUF_2000M_TS2.md); supply/terminals pp. 5, 7; section 7, Modbus pp. 39-45 |
| TUF-2000 series installation | [User manual](tuf-2000m_ts2/TUF-2000_Series_Ultrasonic_Flowmeter_User_Manual.pdf) | English; revision not identified; 35 pages | [TUF notes](../TUF_2000M_TS2.md); TS-2 selection p. 4; installation checks pp. 33-34 |
| RD-RWG-01 level/pressure sensor | [HONDE user manual](RD-RWG-01/RD-RWG-01_water_pressure_user_manual.pdf) | English; revision not identified; 9 pages | [RD-RWG-01 notes](../RD_RWG_01.md); ratings p. 2, wiring p. 3, protocol pp. 3-9 |
| Epever LS1024B | [LS-B manual](Epever_LS1024B/Epever_LS1024B_manual.pdf) | English; V3.3; 2 PDF pages / 4 printed pages | [LS1024B notes](../EPEVER_LS1024B.md); sections 1, 3, 5, 6, 8. PWM controller; does not satisfy the production MPPT requirement |
| IrriRich regular 3-way solenoid (filename says latch) | [Supplied product-page PDF](Irririch_3w_latch_solenoid/irririch_3w_latch_solenoid.pdf) | English; web print 2026-09-29; revision not identified; 2 PDF pages, footers 1/4 and 2/4 | [IrriRich notes](../IRRIRICH_3W_SOLENOID.md); regular-model specifications pp. 1-2. LATCH appears only as a related product; this is not the installed latching-coil specification |
| DELIXI CDI-E family | [Operating manual](DelixiCDIE100/delixi-instrukciya-po-ekspluatacii.pdf) | Russian; revision not identified; 269 pages | [CDI-E100 notes](../DELIXI_CDI_E100.md); P4.1 pp. 174-175; chapter 8, especially pp. 235-240 |
| INVT GD200A | [Manual 2](INVT_GD200A/invt-gd200a-user-manual2.pdf) | English; 201908 (V2.4), document 66001-00342; 179 pages | [GD200A notes](../INVT_GD200A.md); chapter 9; control/status/register tables pp. 143-146 |
| INVT Goodrive200, different model family | [Manual 1](INVT_GD200A/invt-gd200a-user-manual1.pdf) | English; revision not identified; 252 pages | Preface p. 2 identifies **Goodrive200**, despite the GD200A filename. Related reference only; do not substitute its parameters for GD200A |

Supporting images: the [TUF source list](../TUF_2000M_TS2.md#preserved-sources)
links the module, transducer and range images. The RD-RWG-01
[usage illustration](RD-RWG-01/usage.jpg) is a supplied product/application
illustration, not a photograph or commissioning record of the installed unit.

## Supplied hardware requirements

| Source | Language / identification | Current interpretation |
|---|---|---|
| [Soil-node HRS](hardware_requirements/Hardware_Requirements_AmudarIO_Soil_Node.pdf) | English; supplied 2026-08-29; 3 pages | [Current soil PCB requirements v1.2](../AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md) |
| [Controller/hub HRS](hardware_requirements/Hardware_Requirements_Irrigation_Controller_Hub.pdf) | English; supplied 2026-08-29; 3 pages | [Current universal PCB requirements v1.2](../AmudarIO_Universal_12V_PCB_Design_Requirements_v1_2_EN.md) |

Original filenames and hashes are in the [HRS source record](hardware_requirements/README.md).
These source requirements do not establish released schematics or validated PCBs.

## Maintaining these references

- Keep original PDFs/images intact and use exact relative links when moving them.
- Keep device notes focused on integration facts, with four distinct categories:
  **manual facts**, **project implementation/configuration**,
  **hardware-confirmed observations**, and **unresolved items**.
- Cite the manual filename/revision and PDF page or section for technical facts.
  Record test conditions or the user confirmation for hardware observations.
  Defaults, examples and source-code constants are not commissioning evidence.
- When needed, extract text with page markers into `tmp/pdfs/` and search it.
  Inspect the original page for tables, wiring, signs, units and register order.
  Do not routinely load complete manuals/extractions or generated `output/` files.
- Update this index and affected device notes together when a source changes.
  Keep unknowns explicit; summaries must not silently reconcile contradictory sources.
