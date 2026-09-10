"""English contractor copies; preserve the reviewed C6-P1/S3-P2 allocations."""
import re
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from build_pcb_specifications_v1 import Spec, ROOT, C6, S3, EXP, source_table

DESCRIPTIONS = {
    'VBAT_ADC': 'Battery voltage, ADC1_CH0',
    'VBAT_SENSE': 'Battery voltage, ADC1_CH0',
    'VBAT_DIV_EN': 'Battery divider high-side switch enable',
    'SENSOR_DOMAIN_EN': 'Shared enable for 5 V boost and RS-485 power',
    'LORA_RXEN': 'LoRa RF switch receive control',
    'LORA_TXEN': 'LoRa RF switch transmit control',
    'LORA_RESET_N': 'LoRa reset, active LOW',
    'LORA_SCK': 'LoRa SPI clock',
    'LORA_MISO': 'SPI data from LoRa to MCU',
    'LORA_MOSI': 'SPI data from MCU to LoRa',
    'LORA_NSS': 'LoRa SPI chip select, active LOW',
    'LORA_DIO1': 'LoRa interrupt',
    'LORA_BUSY': 'LoRa busy status',
    'RESERVED_STRAP_MTMS': 'Reserved SDIO strap / MTMS',
    'RESERVED_STRAP_MTDI': 'Reserved SDIO strap / MTDI',
    'RESERVED_BOOT_STRAP': 'Reserved boot strap',
    'RESERVED_JTAG_STRAP': 'Reserved JTAG source-selection strap',
    'RESERVED_VDD_SPI_STRAP': 'Reserved flash-supply selection strap',
    'SERVICE_WAKE_N': 'Wake button, connects to GND',
    'BOOT_N': 'BOOT button, programming and recovery only',
    'RS485_TX': 'UART1 TX to automatic-direction RS-485',
    'RS485_RX': 'UART1 RX from automatic-direction RS-485',
    'RS485_UART_TX': 'UART1 TX to RS-485 branch selector',
    'RS485_UART_RX': 'UART1 RX from RS-485 branch selector',
    'UART0_TX': 'UART0 TX, logs and external USB-to-TTL',
    'UART0_RX': 'UART0 RX, programming and Serial commands',
    'VALVE1_IPROPI': 'Valve 1 current sense, ADC1_CH1',
    'VALVE2_IPROPI': 'Valve 2 current sense, ADC1_CH3',
    'VALVE1_PH': 'Valve 1 pulse polarity',
    'VALVE2_PH': 'Valve 2 pulse polarity',
    'VALVE1_EN': 'Valve 1 pulse enable and duration',
    'VALVE2_EN': 'Valve 2 pulse enable and duration',
    'VALVE1_PWR_EN': 'Valve 1 H-bridge power request',
    'VALVE2_PWR_EN': 'Valve 2 H-bridge power request',
    'I2C_SCL': 'I2C clock, expander and local connector',
    'I2C_SDA': 'I2C data, expander and local connector',
    'RESERVED_USB_DM': 'Reserved USB D-, no power-control function',
    'RESERVED_USB_DP': 'Reserved USB D+, no power-control function',
    'IO_EXP_INT_N': 'I/O expander interrupt',
    'RESERVED_JTAG_MTCK': 'Reserved JTAG TCK',
    'RESERVED_JTAG_MTDO': 'Reserved JTAG TDO',
    'RESERVED_JTAG_MTDI': 'Reserved JTAG TDI',
    'RESERVED_JTAG_MTMS': 'Reserved JTAG TMS',
    'POWER_FAULT_N': 'Power supervisor fault input',
}
EXP_DESCRIPTIONS = [
    'Branch address bit 0', 'Branch address bit 1',
    'Connect selected UART branch; NOT DE/RE',
    'RS485-1 field-device power enable', 'RS485-2 field-device power enable',
    'RS485-3 field-device power enable', 'RS485-4 field-device power enable',
    '5 V buck converter enable', 'Auxiliary 5 V connector switch',
    'Auxiliary VBAT connector switch', 'External local I2C power and signal isolation',
    'Battery divider switch', 'Permission to release H-bridge 1 nSLEEP',
    'Permission to release H-bridge 2 nSLEEP', 'Global hardware-interlock permission',
    'Isolated low-voltage equipment control output', 'Protected digital state input 1',
    'Protected digital state input 2', 'Protected digital state input 3',
    'Protected digital state input 4', 'Driver 1 nFAULT', 'Driver 2 nFAULT',
    'Protected auxiliary 5 V output fault', 'Protected auxiliary VBAT output fault',
]

class EnglishSpec(Spec):
    def __init__(self, stem, title, board, pinout):
        super().__init__(stem, title, f'AmudarIO  |  Board {board}  |  Version 1.0  |  4 September 2026  |  Pinout {pinout}')
        for section in self.doc.sections:
            section.footer.paragraphs[0].runs[0].text = 'Version 1.0  |  '
        self.doc.core_properties.subject = 'PCB design technical specification'
        self.doc.core_properties.keywords = 'PCB, technical specification, version 1.0, English'
        self.doc.core_properties.language = 'en-US'
        for style in self.doc.styles:
            if style.type == 1:
                rpr = style.element.get_or_add_rPr()
                lang = OxmlElement('w:lang'); lang.set(qn('w:val'), 'en-US'); rpr.append(lang)
    def save(self):
        body = '\n'.join(self.md)
        assert not re.search(r'[\u0400-\u04FF]', body), 'Untranslated text found'
        assert not re.search(r'\bDRAFT\b|\bTBD\b|\bPROPOSED\b', body, re.I)
        super().save()

def gpio(s, rows):
    translated = [r[:4] + [DESCRIPTIONS[r[3]]] for r in rows]
    s.table(['GPIO','Pad','I/O','Net','Function'], translated, [.43,.43,.35,2.35,3.74], 9)

def radio(s, soil):
    filename = 'SOIL_NODE_ESP32C6_PINOUT_RU.md' if soil else 'UNIVERSAL_12V_ESP32S3_PINOUT_RU.md'
    header = '| Сигнал LoRa |' if soil else '| Сигнал |'
    s.table(['Signal','DX-LR30 pin','ESP32 GPIO'], source_table(filename,header), [2.5,2.4,2.4])
    s.p('DX-LR30 supply: 3.3 V, pin 9 in the project reference. Leave DIO2/DIO3 unconnected. Verify the purchased module marking and revision drawing before assigning its footprint. LoRa module pin numbers are not ESP32 GPIO numbers.')

def programming(s, tx, rx, boot, en):
    s.table(['Pin','Board signal','Adapter connection'], [
        ['1','GND','GND'], ['2','3V3_REF','High-impedance reference input only; otherwise leave open'],
        ['3',f'UART0_TX / GPIO{tx}','Adapter RX'], ['4',f'UART0_RX / GPIO{rx}','Adapter TX'],
        ['5','DTR','Through the two-transistor auto-reset circuit'],
        ['6','RTS','Through the two-transistor auto-reset circuit'],
    ], [.65,2.25,4.4])
    s.p(f'Use 3.3 V logic only. Power the board from its own source; do not connect the adapter 3.3 V, 5 V or VBUS power outputs. BOOT: GPIO{boot}; RESET: EN, module pad {en}. Route DTR/RTS through the standard Espressif circuit, never directly to BOOT/EN. With a TX/RX/GND-only adapter, use the BOOT and RESET buttons. The same UART0 supports Serial Monitor; verify DTR/RTS behavior when opening the port.')

def soil():
    s = EnglishSpec('AmudarIO_Soil_Node_Technical_Specification_v1_0_EN','Soil node PCB technical specification',1,'C6-P1')
    s.p('Design an autonomous PCB for one external 5 V RS-485 soil probe. The node measures soil temperature, volumetric water content, electrical conductivity and battery voltage, and sends the results to ChirpStack using LoRaWAN Class A.')
    s.h('1 Hardware and architecture')
    s.table(['Block','Implementation'], [
        ['MCU','ESP32-C6-MINI-1-H4, 4 MB flash'],
        ['LoRa','SX1262-based DX-LR30-900M22S; external antenna through SMA'],
        ['Battery','Removable protected 18650 Li-ion cell, 1S, 3.6/3.7 V nominal, 4.2 V charge'],
        ['Holder','MPD BK-18650-PC2 with retainer; holder included in PCBA, cell supplied separately'],
        ['Solar charging','LTC4079 with hardware NTC in physical contact with the cell'],
        ['Always-on 3.3 V','TPS63900 buck-boost for ESP32 and LoRa'],
        ['Switched 5 V probe rail','TPS61023 with EN and true output disconnect'],
        ['RS-485','THVD1406 automatic direction; power gated by TPS22917'],
        ['Battery measurement','Internal ADC; divider with TPS22917 high-side switch'],
        ['Service','External USB-to-TTL on UART0; BOOT, RESET and SERVICE WAKE'],
    ],[1.85,5.45])
    s.p('A nominal 6 V solar panel feeds the charger. The protected battery feeds the 3.3 V and 5 V converters through a mechanical SYSTEM switch. This switch disconnects all system loads; the charger and NTC remain connected to the panel and battery.')
    s.p('Do not fit GPS, an external RTC, ADS1115, a fuel gauge, continuously lit LEDs or an onboard USB-UART bridge. Disable Wi-Fi, BLE and 802.15.4 in deployed firmware. LiFePO4 is not an interchangeable battery option.')
    s.h('2 Operating cycle')
    s.p('Timer wake; safe initialization; enable probe and RS-485; wait for startup; read Modbus with bounded retries; disable the probe; measure battery voltage; send uplink; open RX1/RX2; process a command and acknowledge immediately; shut down LoRa; enter deep sleep. The server can change the interval, which must be retained. Do not execute a repeated command with the same ID.')

    s.page(); s.h('3 Complete ESP32 C6 pinout')
    s.p('GPIO is the firmware identifier. Pad is the physical ESP32-C6-MINI-1-H4 module pad, not a bare-chip pin or DevKit header position. A = ADC input, I = input, O = output. All logic signals use 3.3 V levels.')
    gpio(s,C6)
    s.p('Power: pad 3 = 3.3 V; pad 8 = EN/CHIP_PU. GND: pads 1, 2, 11, 14, 36-53. NC: pads 4, 7, 21, 32-35. GPIO10/11 are not exposed. No general-purpose GPIOs remain unallocated. Fit an EN RC network and supervisor suitable for slow and interrupted power ramps.')
    s.p('GPIO1 enables only the battery-divider switch. GPIO2 enables the 5 V boost and RS-485 power switch together. GPIOs must not carry load current or connect directly to the battery, 5 V or A/B lines.')

    s.page(); s.h('4 GPIO restrictions and safe states')
    s.bullets(
        'GPIO4/5/8/9/15 are strapping pins. Do not load GPIO4/5. Pull GPIO8/9 to 3.3 V through 10 kOhm and GPIO15 to GND through 10 kOhm. UART download requires GPIO8=1 and GPIO9=0 at reset; normal boot requires GPIO9=1.',
        'GPIO6/7 reuse alternate JTAG functions. Do not enable external pad JTAG on GPIO4-7. Check the factory eFuse configuration; do not burn eFuses to accommodate the pinout.',
        'GPIO12/13 default to USB D-/D+. Disable USB functions and pulls, then assign UART1 before enabling RS-485. BOTH UART paths need powered-off isolation with specified Ioff. A series resistor alone is insufficient.',
        'Reserve GPIO16/17 for UART0. ROM TX logs must not control loads. GPIO18-23 are valid for LoRa, but require explicit SPI routing and consideration of startup SDIO pulls.',
    )
    s.table(['Signals','Reset and startup','Deep sleep'],[
        ['GPIO1/2','LOW, external PD about 100 kOhm','LOW and held; divider and sensor domain OFF'],
        ['GPIO3/7 RXEN/TXEN','LOW, PD about 100 kOhm','LOW and held'],
        ['GPIO14/21 RESET/NSS','HIGH, PU 47-100 kOhm','HIGH after radio shutdown'],
        ['GPIO18/20 SCK/MOSI','Keep NSS HIGH','LOW and held'],
        ['GPIO6 WAKE','PU about 100 kOhm; button to GND','Wake input; HIGH when idle'],
        ['ADC and other inputs','No overvoltage or back-power','Disable unused input buffers'],
    ],[2.25,2.5,2.55])
    s.p('External circuitry must establish these states; firmware alone is insufficient. Before sleep, disconnect UART paths, set GPIO2/1 and RXEN/TXEN LOW, and configure hold. On wake, establish safe levels before releasing hold. Confirm pull values against leakage and noise margins.')
    s.h('5 LoRa connections'); radio(s,True)

    s.page(); s.h('6 Power charging and battery measurement')
    s.p('Configure LTC4079 for 4.2 V and fixed input-voltage regulation based on the selected panel Vmp. It is a linear charger with Vmp regulation, not dynamic MPPT. Charge current must not exceed 250 mA or the cell, panel and thermal limits. Verify weak-light startup and charge termination with the node operating. Hardware NTC qualification must inhibit charging outside the cell temperature limits and on NTC faults.')
    s.p('If winter energy is insufficient, LTC4121-4.2 with fractional-Voc MPPT is an approval-required alternative after recalculating sleep current. Cell overcharge, over-discharge, overload and short-circuit protection are mandatory independently of the charger. Provide reverse-insertion protection, a cell retainer and an NTC contacting the cell sidewall.')
    s.p('Validate TPS63900 at minimum battery voltage and ESP32/LoRa peak load. The TPS61023 stage must disconnect and discharge its 5 V output; protect against shorts and reverse back-power. All disabled interfaces must remain high impedance.')
    s.p('Switch the battery divider on its high side. Starting values: 499 + 499 kOhm upper leg, 249 kOhm lower leg, 100 nF from ADC to GND. GPIO1 enables the switch; GPIO0 reads ADC1_CH0. Wait at least five RC time constants, discard the first sample, average readings and disable the switch. Confirm values for leakage, ADC loading and temperature; perform two-point calibration. Report voltage and normal/low/critical states, not an accurate state-of-charge percentage.')
    s.h('7 Connectors and RS485')
    s.p('Probe connector: 1 = +5V_SOIL_SW; 2 = GND/reference; 3 = RS485_A; 4 = RS485_B. Panel connector: PV+ and PV-. Use keyed, retained connectors with polarity labels. Select the connector family for the enclosure and cable.')
    s.p('THVD1406 direction is automatic; no MCU DE/RE is allocated. At the connector, fit an SM712-class TVS and footprints for surge-rated series resistors, optional 120 ohm termination and bias. Select values and termination population for the cable; power bias from the switched domain. Validate 4800/9600 baud, bus release timing and Modbus with the actual probe.')
    s.h('8 Programming and Serial Monitor'); programming(s,16,17,9,8)

    s.page(); s.h('9 PCB and power consumption requirements')
    s.p('Baseline: two-layer FR-4 with a nearly continuous ground plane. Four layers require a routing or EMC justification. ANT-to-SMA must be a short 50 ohm controlled-impedance trace calculated by the fabricator for the actual stackup. This is trace geometry, not a 50 ohm resistor. Keep power loops and inductors away from RF/ADC, respect the ESP32 antenna keepout, and locate protection at the connectors.')
    s.table(['Mode','Requirement'],[
        ['Normal timer deep sleep','Complete assembled-board current no greater than 25 uA'],
        ['Inactive state and error backoff','Below 1 mA; probe, RS-485 and divider OFF'],
        ['SYSTEM OFF','System loads unpowered; charging qualified by hardware NTC'],
        ['Sensing, boot, LoRa TX/RX, programming, charging','Active modes; sleep-current limits do not apply'],
    ],[2.6,4.7])
    s.p('Measure at the battery input with SYSTEM ON, panel and programmer disconnected, LEDs off, LoRa shut down and probe power removed. Use 3.70 V as the reference point; repeat at voltage and temperature limits. Also test with the unpowered probe connected. Include the cell protector, charger, supervisor, pulls and leakage in the budget. Provide a removable current-measurement link. Bound join/Modbus retries, then enter sleep.')
    s.h('10 Verification and contractor deliverables')
    s.bullets(
        'Verify power-up, BOOT/RESET, programming, watchdog and brownout without unintended probe power. Verify no back-power through UART or A/B.',
        'Test charging, NTC faults and temperature, 3.3 V under RF peaks, 5 V during probe startup and short circuit, ADC, Modbus, LoRaWAN join/uplink/RX1/RX2/ACK, sleep and wake.',
        'Deliver native EDA files and libraries, schematic PDF, BOM with exact manufacturer part numbers, Gerber/NC drill, assembly drawings and pick-and-place, STEP, ERC/DRC, power/RF/autonomy calculations, test firmware and measurement reports.',
    )
    s.h('11 Design inputs and component selection')
    s.p('Before fabrication, record the protected 18650 model and dimensions, NTC and temperature limits; panel Voc/Vmp/Isc and power; probe model, Modbus map, current and startup time; DX-LR30 revision and regional plan; enclosure, outline, connectors and cables; winter autonomy, intervals, battery thresholds, rail/ADC tolerances and EMC/ESD levels. The contractor shall calculate component values and obtain customer approval.')
    s.p('Component references: Espressif ESP32-C6-MINI-1 datasheet and Hardware Design Guidelines; Analog Devices LTC4079; Texas Instruments TPS63900, TPS61023, TPS22917 and THVD1406. The pin-map identifier for this specification is C6-P1.')
    s.save()

def universal():
    s=EnglishSpec('AmudarIO_Universal_12V_Controller_Technical_Specification_v1_0_EN','Universal 12 V controller PCB technical specification',2,'S3-P2')
    s.p('Design one PCB with assembly variants for two latching valves, RS-485 pressure/flow/level sensors, and a low-voltage main-valve or variable-frequency-drive interface. The board opens and closes valves and measures pressure; closed-loop electronic pressure regulation is not included.')
    s.h('1 Hardware and application profiles')
    s.table(['Block','Implementation'],[
        ['MCU','ESP32-S3-WROOM-1-N8, 8 MB flash, no PSRAM'],
        ['LoRa','SX1262-based DX-LR30, SMA, LoRaWAN Class A or Class C'],
        ['Valves','2 x DRV8874 in PH/EN mode; independent power gates, current sense and nFAULT'],
        ['RS-485','4 protected selectable branches; one active; automatic direction'],
        ['Power','Always-on 3.3 V and switched 5 V using TPS62933; protected VBAT_SW'],
        ['Additional I/O','TCA6424A RGJ at 0x22; local 3.3 V I2C; 4 protected state inputs'],
        ['Service','External USB-to-TTL, UART0, DTR/RTS, BOOT/RESET'],
    ],[1.55,5.75])
    s.table(['Profile','Purpose','Mode'],[
        ['DUAL_PCV','Two valves, upstream/downstream pressure, TUF-2000M','Class A or C'],
        ['LEVEL_FLOW','Level/flow sensors; H-bridges not populated','Usually Class A'],
        ['MAIN_VALVE','Main valve, external 12 V supply','Usually Class C'],
        ['PUMP_VFD','VFD RS-485, status and interlock inputs','Class C'],
    ],[1.5,4.4,1.4])
    s.h('2 Input power and battery protection')
    s.p('Accept DC only: a 12 V lead-acid battery or an external certified isolated 230 VAC to 12 VDC supply. No mains voltage may enter this PCB. The lead-acid solar charger is external. Check the 10.5-14.8 V design operating range against the selected source.')
    s.p('Provide an input fuse, reverse-polarity and transient/overvoltage protection, hardware UVLO with hysteresis, and POWER_FAULT_N monitoring. TPS37-Q1 is the supervisor baseline, not a power switch. On discharge, warn and inhibit new OPEN commands first, reserve energy for CLOSE/status, then apply the lower hardware cutoff. The suggested 11.5 V is a starting point for validating OPEN inhibit, not a fixed whole-board cutoff. Calculate thresholds and delays separately for BATTERY and EXTERNAL_PSU.')

    s.page(); s.h('3 Complete ESP32 S3 pinout')
    s.p('GPIO is the firmware number. Pad refers to ESP32-S3-WROOM-1-N8. A = ADC input; I = input; O = output; OD = open-drain. All signals use 3.3 V logic. S3-P2 does not describe the classic ESP32 prototype.')
    gpio(s,S3[:22])
    s.h('Module power')
    s.p('Pad 2: 3.3 V; pad 3: EN/CHIP_PU with RC and supervisor. Pads 1, 40 and exposed pad 41: GND. GPIO22-34 are not available as external module GPIOs; do not use internal flash connections.')
    s.p('Assign SPI and UART explicitly through the GPIO matrix. Do not connect any GPIO directly to 5/12/24 V, a power MOSFET gate or RS-485 A/B. Clamp and scale analog inputs to the permitted ADC range.')

    s.page(); s.h('3 Complete ESP32 S3 pinout continued'); gpio(s,S3[22:])
    s.h('4 Pin restrictions')
    s.bullets(
        'GPIO0/3/45/46 are straps. Pull GPIO0/3 HIGH through 10 kOhm and GPIO45/46 LOW through 10 kOhm. GPIO0 is BOOT only. Do not attach field loads; check factory eFuses.',
        'Reserve GPIO39-42 exclusively for JTAG. Reserve GPIO19/20 for USB/debug, not power enables. GPIO43/44 are dedicated UART0; ROM logs must not activate loads.',
        'GPIO35/36/37 are available on the selected no-PSRAM N8. An octal-PSRAM N8R8 substitution changes pin availability. R16V is also not interchangeable: GPIO47/48 operate at 1.8 V. Any module substitution requires a schematic review.',
        'GPIO15/16 are used by LoRa; do not fit a 32.768 kHz crystal. GPIO37/48 are not RTC inputs; baseline Class A uses timer wake.',
    )
    s.h('5 LoRa connections'); radio(s,False)

    s.page(); s.h('6 TCA6424A expander pinout')
    s.p('Package: 32-pin RGJ. VCCI pin 31 and VCCP pin 27: always-on 3.3 V; GND pin 25 and exposed pad: ground; ADDR pin 26: GND, address 0x22. SCL pin 29: GPIO7; SDA pin 30: GPIO8; INT_N pin 32: GPIO37. Connect RESET_N pin 28 to the common reset/watchdog network. Port P13 is not GPIO13.')
    s.table(['Port','Pin','Net','Function'], [r[:3]+[d] for r,d in zip(EXP,EXP_DESCRIPTIONS)], [.48,.48,2.65,3.69],9)
    s.p('P00-P17 require external pull-downs of about 100 kOhm and become outputs after initialization; P20-P27 are protected inputs with pull-ups. Reset makes all ports inputs, but output latches contain ones. First write and verify 0x00 in 0x04/0x05/0x06; then set Configuration 0x0C=0x00, 0x0D=0x00, 0x0E=0xFF. Enable OUTPUT_ARM last, after direct GPIOs are safe.')
    s.p('An MCU restart must not preserve unsafe expander permissions. Internal MCU reset does not necessarily pull EN LOW; hardware interlocks and watchdog must cover this case. GPIO37 cannot wake the MCU from deep sleep. The four state inputs are for contacts, not guaranteed lossless flow-meter pulse counting.')
    s.p('GPIOs can glitch at startup: inhibit H-bridges and RS-485 branches in hardware. Pull LoRa NSS/RESET_N HIGH through 47-100 kOhm and RXEN/TXEN LOW through 100 kOhm; reinitialize the radio after reset. Start with 4.7 kOhm I2C SCL/SDA pull-ups. Verify all pulls for noise and leakage.')

    s.page(); s.h('7 Two independent valve channels')
    s.table(['Function','Valve 1','Valve 2'],[
        ['PH polarity','GPIO5','GPIO38'], ['EN pulse','GPIO6','GPIO47'],
        ['H-bridge power request','GPIO35','GPIO36'], ['nSLEEP permission','TCA6424A P14','TCA6424A P15'],
        ['nFAULT','TCA6424A P24','TCA6424A P25'], ['IPROPI current ADC','GPIO2 / ADC1_CH1','GPIO4 / ADC1_CH3'],
    ],[2.7,2.3,2.3])
    s.p('Configure both DRV8874 devices in hardware for PH/EN mode. Supply each H-bridge through its own high-side switch only during an operation. Hardware permissions must include OUTPUT_ARM, valid power and reset/watchdog. Release nSLEEP only for a powered channel. Fit external pull-downs of about 100 kOhm on PH, EN and power requests. An I2C command alone must not energize a bridge.')
    s.p('Sequence: EN=0; enable selected bridge power; release nSLEEP; wait for stabilization; set PH; generate a bounded EN pulse; EN=0; wait for current decay; nSLEEP=0; power OFF. Actuate valves one at a time. Maximum-pulse supervision and shutdown on a hang must prevent continuous coil energization.')
    s.p('OPEN and CLOSE require opposite polarities. Removing power does not close a latching valve. Determine duration, polarity, current limit and minimum reliable closing voltage using the actual valve. Provide coil-energy handling and short/open-load/overtemperature protection. Without position feedback, record the last issued action only; physical position remains unconfirmed.')
    s.h('8 Controlled outputs and local I2C')
    s.bullets(
        'P07 enables the 5 V converter; P10 separately enables protected AUX 5 V. P11 enables protected AUX VBAT. Label the latter VBAT_SW: on battery power it follows battery voltage, not regulated 12.0 V.',
        'Each output requires a high-side switch, overload/short-circuit and reverse-back-power protection, reset-default OFF behavior and connector protection. Calculate current ratings and protection values from the load, not IC peak ratings.',
        'P12 enables external I2C power and SDA/SCL isolation. Connector: 3V3_SW, GND, SDA, SCL. Keep the expander and internal pulls on always-on 3.3 V. I2C is for short enclosure-local wiring only.',
        'P13 enables the divider high-side switch; GPIO1 measures battery voltage. Provide RC filtering, ADC clamping, two-point calibration and divider shutdown after reading. Report voltage, low/inhibit status and reset cause, not an accurate charge percentage.',
    )
    s.p('Protect and level-condition field inputs; select dry-contact or 12/24 V input circuits by assembly profile. P17 controls only an isolated low-voltage equipment permission. It does not replace emergency stop; the VFD safe-stop chain must operate independently of MCU, I2C and LoRa.')

    s.page(); s.h('9 RS485 and field-device power')
    s.p('UART1 TX=GPIO18, RX=GPIO21. Connect four branches through a break-before-make selector; never tie push-pull receiver outputs together. SEL1:SEL0 = 00/01/10/11 selects RS485-1/2/3/4. P02=0 disconnects all branches; it enables the selector and is not DE/RE. P03-P06 request port power; a hardware decoder permits only the selected branch.')
    s.table(['Port','DUAL_PCV role','Power'],[
        ['RS485-1','Upstream pressure','5 V or VBAT according to the sensor'],
        ['RS485-2','Downstream pressure','5 V or VBAT according to the sensor'],
        ['RS485-3','TUF-2000M','VBAT; manual specifies 8-36 V, about 50 mA'],
        ['RS485-4','Spare level/flow/equipment port','Fixed by assembly profile'],
    ],[1.05,2.75,3.5])
    s.p('Before switching, finish the transaction, set P02=0 and remove old-branch power. Select the new port, enable its domain, wait for startup, then permit communication. Only one branch is active in the baseline. Fix 5 V/VBAT selection in hardware at assembly; software must not be able to apply VBAT to a 5 V sensor.')
    s.p('Direction is automatic. Use the MAX22025F/MAX22026F isolated-transceiver family as the baseline with a separate cable-side supply. The contractor shall select the exact variant and verify required baud rates, bus release, EMC and shutdown current. Changing to manual DE/RE requires approval.')
    s.p('Full galvanic isolation requires isolation of both signals AND sensor power/return. For a board-powered sensor, use a suitably rated isolated DC/DC; a common VBAT/5 V ground defeats full isolation. Identify FULL_ISOLATION and SIGNAL_ISOLATION_ONLY separately in the BOM. Do not connect the VFD field return to board logic ground.')
    s.p('Each port provides PWR, RETURN, A, B and SHIELD; keying, voltage labels, current protection, TVS, optional 120 ohm termination and bias footprints. Set isolation ratings, clearances and shield termination for the installation. Disable the transceiver and isolated DC/DC outside sessions; prevent back-power through TX/RX, A/B and fault lines.')
    s.h('10 Flow-meter compatibility')
    s.p('Retain the commissioned TUF-2000M settings: address 1; 9600 8N1; Modbus RTU; REAL4 LOW_WORD_FIRST. Use function 03 for flow REG0001-0002, velocity REG0005-0006, errors REG0072 and accumulated volumes REG0113-0118. Request addresses are one less than the REG number. A delivered-volume reset stores a baseline in ESP32 NVS; do not modify meter totalizers or menu M37.')
    s.p('Measure TUF power-on readiness and qualify power cycling. An unpowered meter cannot account for passing water; continuous totalization requires a separate always-powered profile and a review of sequential branch power. Select RS-485 pressure sensors using their own manuals; do not reuse the old I2C sensor scaling.')

    s.page(); s.h('11 Programming operating modes and safe sleep'); programming(s,43,44,0,3)
    s.p('Serial-only keeps the MCU awake for local commands and does not start LoRa. Class C keeps the MCU and LoRa receiving; its interval is a telemetry period, not sleep time. Class A uses timer wake, measurement/action, uplink, RX1/RX2, immediate command acknowledgement and real deep sleep; it cannot receive commands at arbitrary times. Keep valve and VFD firmware separate.')
    s.p('Before Class A sleep: EN/PH=0, nSLEEP=0, bridge power requests=0; set P00-P17 to safe OFF; shut down LoRa, RXEN/TXEN=0, NSS/RESET=1, SCK/MOSI=0. Use supported digital-pad deep-sleep hold and external pulls for direct outputs; set safe levels before releasing hold on wake. An I2C error must immediately drop direct EN and bridge power requests.')
    s.p('Design the minimal Class A assembly toward no more than 50 uA; establish acceptance limits for each assembly using maximum leakage of all populated circuits. This is not a guaranteed full-board current. Measure at the 12 V input without a programmer or active sensors; include TCA6424A, pulls, switches, supervisor, LoRa and DC/DC. Class C and Serial-only are not sleep modes. Provide a removable current-measurement link; omit continuously lit LEDs.')
    s.h('12 PCB verification and contractor deliverables')
    s.p('Maximum-feature assembly: four-layer FR-4, continuous reference GND respecting isolation clearances, and short separate H-bridge power loops. Route ANT-to-SMA as a calculated 50 ohm line; keep power returns out of RF/ADC paths. A two-layer option requires routing and EMC review. Label connector voltages and polarities.')
    s.p('Acceptance: ERC/DRC; power-up/BOOT/reset/watchdog/brownout without valve pulses; short/open-load/thermal tests per output; no simultaneous valve pulses; reliable OPEN/CLOSE; UVLO/recovery; four RS-485 branches; TUF; LoRaWAN; I2C stuck-low; no back-power; sleep current by profile; EMC/ESD and communication-loss behavior. Deliver EDA files and libraries, schematic PDF, BOM/MPNs and assembly profiles, Gerber/drill, assembly files, STEP, calculations, test firmware and reports.')
    s.h('13 Design inputs and component selection')
    s.p('Before fabrication, record valve models, pulse duration/current/polarity and cables; sensors and Modbus maps; 5 V/VBAT load currents; isolation and DC/DC per port; battery/charger and UVLO; VFD, feedback and safe-stop requirements; enclosure, connectors, temperature and EMC; regional plan and DX-LR30 revision. The contractor shall obtain customer approval for exact MPNs and calculated values. Port production firmware to S3-P2 separately from the working classic ESP32 prototype.')
    s.p('Pinout references: Espressif ESP32-S3-WROOM-1 datasheet v1.8 and Hardware Design Guidelines; TI TCA6424A Rev D and DRV8874. All 36 module GPIOs and 24 expander ports are listed in this specification.')
    s.save()

if __name__ == '__main__':
    soil()
    universal()
