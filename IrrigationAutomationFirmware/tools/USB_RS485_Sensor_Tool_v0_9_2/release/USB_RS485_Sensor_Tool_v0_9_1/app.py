#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USB-RS485 Sensor Tool for Amudario sensors.
Standalone Tkinter desktop utility for Windows/Linux/macOS.

Main features:
- COM port connect/disconnect
- Modbus RTU raw TX/RX with CRC16
- Device profile based scan/read
- Safe address change wizard
- JSON/CSV/log export
"""

from __future__ import annotations

import csv
import json
import math
import os
import queue
import struct
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

try:
    import serial
    from serial.tools import list_ports
    SERIAL_AVAILABLE = True
except Exception:
    serial = None
    list_ports = None
    SERIAL_AVAILABLE = False

APP_NAME = "USB-RS485 Sensor Tool"
APP_VERSION = "0.9.1"
APP_TITLE = f"{APP_NAME} v{APP_VERSION}"

BAUD_RATES = [4800, 9600, 115200]
DEFAULT_TIMEOUT_MS = 1500
DEFAULT_RETRIES = 3
DEFAULT_POST_DELAY_MS = 20
DEFAULT_SCAN_GAP_MS = 5


# -----------------------------------------------------------------------------
# Utility functions
# -----------------------------------------------------------------------------

def app_base_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def resource_path(name: str) -> Path:
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        return Path(sys._MEIPASS) / name  # type: ignore[attr-defined]
    return app_base_dir() / name


def now_local_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def now_log_time() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def bytes_to_hex(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def clean_hex_string(text: str) -> str:
    # Accept spaces, commas, semicolons, tabs, newlines and optional 0x prefixes.
    text = text.replace(",", " ").replace(";", " ").replace("\n", " ").replace("\r", " ").replace("\t", " ")
    parts = []
    for token in text.split():
        token = token.strip()
        if token.lower().startswith("0x"):
            token = token[2:]
        if token:
            parts.append(token)
    return " ".join(parts)


def parse_hex_string(text: str) -> bytes:
    cleaned = clean_hex_string(text)
    if not cleaned:
        return b""
    out = bytearray()
    for token in cleaned.split():
        if len(token) > 2:
            raise ValueError(f"Invalid byte token '{token}'. Use bytes like 01 03 00 00.")
        value = int(token, 16)
        if not 0 <= value <= 255:
            raise ValueError(f"Byte value out of range: {token}")
        out.append(value)
    return bytes(out)


def parse_address(value: str) -> int:
    s = value.strip()
    if not s:
        raise ValueError("Address is empty")
    if s.lower().startswith("0x"):
        addr = int(s, 16)
    else:
        addr = int(s, 10)
    if not 0 <= addr <= 247:
        raise ValueError("Address must be in range 0..247")
    return addr


def format_address(addr: int) -> str:
    return f"0x{addr:02X} ({addr})"


def int16_from_register(hi: int, lo: int) -> int:
    value = (hi << 8) | lo
    if value & 0x8000:
        value -= 0x10000
    return value


def uint16_from_register(hi: int, lo: int) -> int:
    return (hi << 8) | lo


def modbus_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_crc(data: bytes) -> bytes:
    crc = modbus_crc16(data)
    return data + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def u16_to_bytes(value: int) -> bytes:
    value = int(value) & 0xFFFF
    return bytes([(value >> 8) & 0xFF, value & 0xFF])


def build_fc03(address: int, start_register: int, count: int) -> bytes:
    return bytes([address & 0xFF, 0x03]) + u16_to_bytes(start_register) + u16_to_bytes(count)


def build_fc04(address: int, start_register: int, count: int) -> bytes:
    return bytes([address & 0xFF, 0x04]) + u16_to_bytes(start_register) + u16_to_bytes(count)


def build_fc10(address: int, start_register: int, values: List[int]) -> bytes:
    count = len(values)
    if count <= 0:
        raise ValueError("FC10 requires at least one register value")
    payload = b"".join(u16_to_bytes(v) for v in values)
    return (
        bytes([address & 0xFF, 0x10])
        + u16_to_bytes(start_register)
        + u16_to_bytes(count)
        + bytes([len(payload)])
        + payload
    )


def parse_registers_from_fc03_fc04_frame(frame: bytes) -> List[int]:
    if len(frame) < 5:
        raise ValueError("Frame too short")
    byte_count = frame[2]
    if byte_count % 2 != 0:
        raise ValueError("Register byte count is odd")
    if len(frame) < 3 + byte_count + 2:
        raise ValueError("Frame shorter than byte count")
    data = frame[3:3 + byte_count]
    return [uint16_from_register(data[i], data[i + 1]) for i in range(0, len(data), 2)]


def validate_crc(frame: bytes) -> bool:
    if len(frame) < 4:
        return False
    body = frame[:-2]
    received = frame[-2] | (frame[-1] << 8)
    return modbus_crc16(body) == received


def replace_template_tokens(template: str, address: int, new_address: Optional[int] = None) -> bytes:
    out: List[int] = []
    for token in template.strip().split():
        upper = token.upper()
        if upper == "AA":
            out.append(address & 0xFF)
        elif upper == "NN":
            if new_address is None:
                raise ValueError("Template requires NN new address, but new_address was not provided")
            out.append(new_address & 0xFF)
        else:
            if upper.startswith("0X"):
                upper = upper[2:]
            out.append(int(upper, 16) & 0xFF)
    return bytes(out)


def expected_prefix_from_template(prefix_template: str, address: int, new_address: Optional[int] = None):
    pattern: List[Optional[int]] = []
    has_wildcard = False
    for token in prefix_template.strip().split():
        upper = token.upper()
        if upper in ("XX", "??", "*"):
            pattern.append(None)
            has_wildcard = True
        elif upper == "AA":
            pattern.append(address & 0xFF)
        elif upper == "NN":
            if new_address is None:
                raise ValueError("Template requires NN new address, but new_address was not provided")
            pattern.append(new_address & 0xFF)
        else:
            if upper.startswith("0X"):
                upper = upper[2:]
            pattern.append(int(upper, 16) & 0xFF)
    if has_wildcard:
        return pattern
    return bytes([b for b in pattern if b is not None])


def prefix_matches(window: bytes, expected_prefix) -> bool:
    if isinstance(expected_prefix, (bytes, bytearray)):
        return window.startswith(bytes(expected_prefix))
    if isinstance(expected_prefix, list):
        if len(window) < len(expected_prefix):
            return False
        for idx, value in enumerate(expected_prefix):
            if value is not None and window[idx] != value:
                return False
        return True
    return False


def find_frame(rx: bytes, expected_prefix, expected_length: int) -> Tuple[Optional[bytes], Optional[int], str]:
    if expected_length <= 0:
        return None, None, "Expected length is invalid"
    if len(rx) < expected_length:
        return None, None, f"RX shorter than expected frame length ({len(rx)} < {expected_length})"

    prefix_matches_without_crc: Optional[Tuple[bytes, int]] = None
    for start in range(0, len(rx) - expected_length + 1):
        window = rx[start:start + expected_length]
        if not prefix_matches(window, expected_prefix):
            continue
        if validate_crc(window):
            return window, start, "CRC OK"
        if prefix_matches_without_crc is None:
            prefix_matches_without_crc = (window, start)

    if prefix_matches_without_crc is not None:
        frame, start = prefix_matches_without_crc
        return frame, start, "CRC ERROR"

    return None, None, "No matching frame prefix found"


def decode_ascii_preview(data: bytes) -> str:
    chars: List[str] = []
    for b in data:
        if b in (10, 13):
            chars.append("\\n")
        elif 32 <= b <= 126:
            chars.append(chr(b))
        else:
            chars.append(".")
    return "".join(chars)


# -----------------------------------------------------------------------------
# Device profiles
# -----------------------------------------------------------------------------

@dataclass
class ParsedValue:
    name: str
    label: str
    value: Any
    unit: str
    raw: Any
    status: str


class DeviceProfileRegistry:
    def __init__(self, path: Path):
        self.path = path
        self.profiles: List[Dict[str, Any]] = []
        self.load()

    def load(self) -> None:
        with open(self.path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self.profiles = data.get("profiles", [])
        if not self.profiles:
            raise RuntimeError("No device profiles loaded")

    def labels(self, include_all: bool = False) -> List[str]:
        labels = [p["label"] for p in self.profiles]
        return (["All supported profiles"] + labels) if include_all else labels

    def by_label(self, label: str) -> Optional[Dict[str, Any]]:
        for p in self.profiles:
            if p.get("label") == label:
                return p
        return None

    def by_id(self, profile_id: str) -> Optional[Dict[str, Any]]:
        for p in self.profiles:
            if p.get("id") == profile_id:
                return p
        return None

    def all(self) -> List[Dict[str, Any]]:
        return list(self.profiles)


class ProfileParser:
    @staticmethod
    def parse(profile: Dict[str, Any], frame: bytes) -> List[ParsedValue]:
        return ProfileParser.parse_spec(profile.get("read", {}), frame)

    @staticmethod
    def parse_spec(read_spec: Dict[str, Any], frame: bytes) -> List[ParsedValue]:
        fields = read_spec.get("fields", [])
        values: List[ParsedValue] = []

        for field in fields:
            try:
                parsed = ProfileParser.parse_field(frame, field)
                values.append(parsed)
            except Exception as exc:
                values.append(ParsedValue(
                    name=field.get("name", "unknown"),
                    label=field.get("label", field.get("name", "Unknown")),
                    value="",
                    unit=field.get("unit", ""),
                    raw="",
                    status=f"PARSE ERROR: {exc}",
                ))
        return values

    @staticmethod
    def unavailable_values(read_spec: Dict[str, Any], status: str) -> List[ParsedValue]:
        return [
            ParsedValue(
                name=field.get("name", "unknown"),
                label=field.get("label", field.get("name", "Unknown")),
                value="",
                unit=field.get("unit", ""),
                raw="",
                status=status,
            )
            for field in read_spec.get("fields", [])
        ]

    @staticmethod
    def parse_field(frame: bytes, field: Dict[str, Any]) -> ParsedValue:
        offset = int(field.get("register_offset", 0))
        data_start = 3
        field_type = field.get("type", "uint16")
        unit = field.get("unit", "")
        scale = float(field.get("scale", 1.0))
        label = field.get("label", field.get("name", "Unknown"))
        name = field.get("name", label)
        status = "OK"

        if field_type == "uint16":
            i = data_start + offset * 2
            raw = uint16_from_register(frame[i], frame[i + 1])
            value = raw * scale
        elif field_type == "int16":
            i = data_start + offset * 2
            raw = int16_from_register(frame[i], frame[i + 1])
            value = raw * scale
        elif field_type == "uint32_low_word_first":
            i = data_start + offset * 2
            low = uint16_from_register(frame[i], frame[i + 1])
            high = uint16_from_register(frame[i + 2], frame[i + 3])
            raw = (high << 16) | low
            value = raw * scale
        elif field_type == "float32_word_reorder":
            # For Honde ultrasonic wind: response data for reg1: D1 D0, reg2: D3 D2,
            # reorder to D3 D2 D1 D0 and decode big-endian IEEE754.
            i = data_start + offset * 2
            if i + 3 >= len(frame):
                raise ValueError("Not enough bytes for float32")
            d1 = frame[i]
            d0 = frame[i + 1]
            d3 = frame[i + 2]
            d2 = frame[i + 3]
            raw_bytes = bytes([d3, d2, d1, d0])
            raw = bytes_to_hex(raw_bytes)
            value = struct.unpack(">f", raw_bytes)[0] * scale
        elif field_type in ("float32_high_word_first", "float32_low_word_first"):
            i = data_start + offset * 2
            if i + 3 >= len(frame):
                raise ValueError("Not enough bytes for float32")
            wire_bytes = bytes(frame[i:i + 4])
            if field_type == "float32_high_word_first":
                ieee_bytes = wire_bytes
            else:
                ieee_bytes = wire_bytes[2:4] + wire_bytes[0:2]
            raw = bytes_to_hex(wire_bytes)
            value = struct.unpack(">f", ieee_bytes)[0] * scale
            if not math.isfinite(value):
                status = "NON-FINITE"
        elif field_type == "uint16_high_byte":
            i = data_start + offset * 2
            register = uint16_from_register(frame[i], frame[i + 1])
            raw = register
            value = ((register >> 8) & 0xFF) * scale
        elif field_type == "uint16_low_byte":
            i = data_start + offset * 2
            register = uint16_from_register(frame[i], frame[i + 1])
            raw = register
            value = (register & 0xFF) * scale
        elif field_type == "tuf2000m_error_bits":
            i = data_start + offset * 2
            raw = uint16_from_register(frame[i], frame[i + 1])
            error_names = [
                "no received signal",
                "low received signal",
                "poor received signal",
                "empty pipe",
                "hardware failure",
                "receiver gain adjusting",
                "frequency output overflow",
                "4-20 mA output overflow",
                "RAM checksum error",
                "main/timer clock error",
                "parameter checksum error",
                "ROM checksum error",
                "temperature circuit error",
                "reserved bit 13",
                "internal timer overflow",
                "analog input over range",
            ]
            active = [name for bit, name in enumerate(error_names) if raw & (1 << bit)]
            value = "System normal" if not active else "; ".join(active)
            status = "OK" if not active else "DEVICE WARNING/ERROR"
        else:
            raise ValueError(f"Unsupported field type: {field_type}")

        invalid_raw = field.get("invalid_raw", None)
        if invalid_raw is not None and raw == invalid_raw:
            status = "INVALID RAW"

        min_value = field.get("min", None)
        max_value = field.get("max", None)
        if isinstance(value, (int, float)) and status == "OK":
            if min_value is not None and value < float(min_value):
                status = "RANGE WARNING"
            if max_value is not None and value > float(max_value):
                status = "RANGE WARNING"

        if isinstance(value, float):
            value_out: Any = round(value, 4)
        else:
            value_out = value

        return ParsedValue(name=name, label=label, value=value_out, unit=unit, raw=raw, status=status)


# -----------------------------------------------------------------------------
# Serial / Modbus transport
# -----------------------------------------------------------------------------

class SerialTransport:
    def __init__(self, log_callback):
        self.ser = None
        self.port: Optional[str] = None
        self.baud: Optional[int] = None
        self.log_callback = log_callback
        self.lock = threading.Lock()

    def list_ports(self) -> List[str]:
        if not SERIAL_AVAILABLE:
            return []
        ports = []
        for p in list_ports.comports():
            desc = p.description or ""
            if desc:
                ports.append(f"{p.device} — {desc}")
            else:
                ports.append(p.device)
        return ports

    @staticmethod
    def normalize_port(port_display: str) -> str:
        if " — " in port_display:
            return port_display.split(" — ", 1)[0].strip()
        return port_display.strip()

    def is_open(self) -> bool:
        return bool(self.ser and self.ser.is_open)

    def ensure_open(self, port_display: str, baud: int, timeout_ms: int) -> None:
        if not SERIAL_AVAILABLE:
            raise RuntimeError("pyserial is not installed. Run INSTALL_REQUIREMENTS.bat first.")
        port = self.normalize_port(port_display)
        if not port:
            raise RuntimeError("COM port is empty")
        with self.lock:
            if self.ser and self.ser.is_open and self.port == port and self.baud == baud:
                return
            self.close_locked()
            self.ser = serial.Serial(
                port=port,
                baudrate=baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.03,
                write_timeout=1.0,
            )
            self.port = port
            self.baud = baud
            try:
                self.ser.reset_input_buffer()
                self.ser.reset_output_buffer()
            except Exception:
                pass

    def close_locked(self) -> None:
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            finally:
                self.ser = None
                self.port = None
                self.baud = None

    def close(self) -> None:
        with self.lock:
            self.close_locked()

    def transaction(
        self,
        port_display: str,
        baud: int,
        request: bytes,
        expected_prefix: Optional[bytes],
        expected_length: Optional[int],
        timeout_ms: int,
        retries: int,
        post_delay_ms: int,
    ) -> Dict[str, Any]:
        self.ensure_open(port_display, baud, timeout_ms)
        last_rx = b""
        last_status = "No attempt"
        elapsed_ms = 0

        with self.lock:
            if not self.ser or not self.ser.is_open:
                raise RuntimeError("Serial port is not open")

            for attempt in range(1, retries + 1):
                try:
                    self.ser.reset_input_buffer()
                except Exception:
                    pass

                start = time.perf_counter()
                self.ser.write(request)
                self.ser.flush()
                if post_delay_ms > 0:
                    time.sleep(post_delay_ms / 1000.0)

                rx = bytearray()
                deadline = time.perf_counter() + timeout_ms / 1000.0
                frame = None
                frame_start = None
                frame_status = ""

                while time.perf_counter() < deadline:
                    available = getattr(self.ser, "in_waiting", 0)
                    if available:
                        chunk = self.ser.read(available)
                    else:
                        chunk = self.ser.read(1)
                    if chunk:
                        rx.extend(chunk)
                        if expected_prefix is not None and expected_length is not None:
                            frame, frame_start, frame_status = find_frame(bytes(rx), expected_prefix, expected_length)
                            if frame is not None and frame_status == "CRC OK":
                                break

                elapsed_ms = int((time.perf_counter() - start) * 1000)
                last_rx = bytes(rx)

                if expected_prefix is not None and expected_length is not None:
                    frame, frame_start, frame_status = find_frame(last_rx, expected_prefix, expected_length)
                    last_status = frame_status
                    if frame is not None and frame_status == "CRC OK":
                        return {
                            "ok": True,
                            "attempt": attempt,
                            "tx": request,
                            "rx": last_rx,
                            "frame": frame,
                            "frame_start": frame_start,
                            "status": frame_status,
                            "elapsed_ms": elapsed_ms,
                        }
                else:
                    last_status = "Raw response received" if last_rx else "No response"
                    if last_rx:
                        return {
                            "ok": True,
                            "attempt": attempt,
                            "tx": request,
                            "rx": last_rx,
                            "frame": None,
                            "frame_start": None,
                            "status": last_status,
                            "elapsed_ms": elapsed_ms,
                        }

            return {
                "ok": False,
                "attempt": retries,
                "tx": request,
                "rx": last_rx,
                "frame": None,
                "frame_start": None,
                "status": last_status,
                "elapsed_ms": elapsed_ms,
            }

    def write_ascii_and_read(self, port_display: str, baud: int, text: str, timeout_ms: int) -> Dict[str, Any]:
        self.ensure_open(port_display, baud, timeout_ms)
        with self.lock:
            if not self.ser or not self.ser.is_open:
                raise RuntimeError("Serial port is not open")
            try:
                self.ser.reset_input_buffer()
            except Exception:
                pass
            payload = text.encode("ascii")
            start = time.perf_counter()
            self.ser.write(payload)
            self.ser.flush()
            rx = bytearray()
            deadline = time.perf_counter() + timeout_ms / 1000.0
            while time.perf_counter() < deadline:
                available = getattr(self.ser, "in_waiting", 0)
                if available:
                    chunk = self.ser.read(available)
                else:
                    chunk = self.ser.read(1)
                if chunk:
                    rx.extend(chunk)
            return {
                "tx": payload,
                "rx": bytes(rx),
                "elapsed_ms": int((time.perf_counter() - start) * 1000),
            }


# -----------------------------------------------------------------------------
# EPEVER G3 config presets extracted from the user's config.h
# -----------------------------------------------------------------------------

EPEVER_PRESETS = {
    "PKCELL 2x 12V9Ah parallel": {
        "capacity_ah": 18,
        "max_charge_current_a": 3.60,
        "rated_voltage_level": 1,
        "battery_type": 0,  # User mode
        "voltage_block": {
            "9003_over_voltage_disconnect_v": 15.80,
            "9004_charging_limit_v": 15.00,
            "9005_over_voltage_reconnect_v": 15.00,
            "9006_equalization_v": 14.40,
            "9007_boost_v": 14.40,
            "9008_float_v": 13.80,
            "9009_boost_reconnect_v": 13.20,
            "900A_low_voltage_reconnect_v": 12.60,
            "900B_under_voltage_recover_v": 12.70,
            "900C_under_voltage_warning_v": 12.00,
            "900D_low_voltage_disconnect_v": 11.80,
            "900E_discharging_limit_v": 11.30,
        },
    },
    "StarOne STR12120 12V12Ah": {
        "capacity_ah": 12,
        "max_charge_current_a": 3.60,
        "rated_voltage_level": 1,
        "battery_type": 0,  # User mode
        "voltage_block": {
            "9003_over_voltage_disconnect_v": 15.80,
            "9004_charging_limit_v": 15.00,
            "9005_over_voltage_reconnect_v": 15.00,
            "9006_equalization_v": 14.40,
            "9007_boost_v": 14.40,
            "9008_float_v": 13.80,
            "9009_boost_reconnect_v": 13.20,
            "900A_low_voltage_reconnect_v": 12.60,
            "900B_under_voltage_recover_v": 12.70,
            "900C_under_voltage_warning_v": 12.00,
            "900D_low_voltage_disconnect_v": 11.80,
            "900E_discharging_limit_v": 11.30,
        },
    },
}

EPEVER_VOLTAGE_ORDER = [
    ("9003", "Over-voltage disconnect", "9003_over_voltage_disconnect_v"),
    ("9004", "Charging limit", "9004_charging_limit_v"),
    ("9005", "Over-voltage reconnect", "9005_over_voltage_reconnect_v"),
    ("9006", "Equalization", "9006_equalization_v"),
    ("9007", "Boost / bulk", "9007_boost_v"),
    ("9008", "Float", "9008_float_v"),
    ("9009", "Boost reconnect", "9009_boost_reconnect_v"),
    ("900A", "Low voltage reconnect", "900A_low_voltage_reconnect_v"),
    ("900B", "Under-voltage recover", "900B_under_voltage_recover_v"),
    ("900C", "Under-voltage warning", "900C_under_voltage_warning_v"),
    ("900D", "Low voltage disconnect", "900D_low_voltage_disconnect_v"),
    ("900E", "Discharging limit", "900E_discharging_limit_v"),
]


def volts_to_epever_raw(value: str) -> int:
    return int(round(float(value) * 100.0))


def amps_to_epever_raw(value: str) -> int:
    return int(round(float(value) * 100.0))


def build_epever_service_find_id_request_no_crc() -> bytes:
    # Proprietary EPEVER service command captured from the official PC tool.
    # Full frame example: F8 45 00 01 01 F8 89 BE
    return bytes([0xF8, 0x45, 0x00, 0x01, 0x01, 0xF8])


def build_epever_service_set_id_request_no_crc(new_address: int) -> bytes:
    # Proprietary EPEVER service command captured from the official PC tool.
    # It is NOT normal Modbus FC06/FC10. Full frame for 0x60:
    # F8 45 00 01 01 60 88 14
    if not 1 <= int(new_address) <= 247:
        raise ValueError("EPEVER new address must be 1..247")
    return bytes([0xF8, 0x45, 0x00, 0x01, 0x01, int(new_address) & 0xFF])


# -----------------------------------------------------------------------------
# Main GUI app
# -----------------------------------------------------------------------------

class UsbRs485SensorTool(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1420x880")
        self.minsize(1200, 760)

        profile_path = resource_path("device_profiles.json")
        self.registry = DeviceProfileRegistry(profile_path)
        self.transport = SerialTransport(self.log)
        self.ui_queue: queue.Queue = queue.Queue()
        self.scan_stop_event = threading.Event()
        self.continuous_stop_event = threading.Event()
        self.last_records: List[Dict[str, Any]] = []

        self._init_style()
        self._init_variables()
        self._build_ui()
        self.refresh_ports()
        self.after(30, self.process_ui_queue)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _init_style(self) -> None:
        self.style = ttk.Style(self)
        # Use the native Windows theme when available. The older clam theme draws
        # selected checkboxes with an X, which is exactly the kind of UI crime that
        # makes a safety wizard look like a malware warning.
        try:
            if "vista" in self.style.theme_names():
                self.style.theme_use("vista")
            elif "xpnative" in self.style.theme_names():
                self.style.theme_use("xpnative")
            elif "clam" in self.style.theme_names():
                self.style.theme_use("clam")
        except Exception:
            pass

        self.configure(bg="#f4f6fb")
        self.style.configure(".", font=("Segoe UI", 9))
        self.style.configure("TFrame", background="#f4f6fb")
        self.style.configure("Top.TFrame", background="#e9edf5")
        self.style.configure("Card.TLabelframe", background="#f4f6fb")
        self.style.configure("Card.TLabelframe.Label", font=("Segoe UI", 9, "bold"), foreground="#1f2937")
        self.style.configure("Title.TLabel", font=("Segoe UI", 13, "bold"), foreground="#111827", background="#f4f6fb")
        self.style.configure("Hint.TLabel", foreground="#4b5563", background="#f4f6fb")
        self.style.configure("Status.TLabel", foreground="#0f5132", background="#e9edf5", font=("Segoe UI", 9, "bold"))
        self.style.configure("Info.TLabel", foreground="#1f2937", background="#f4f6fb")
        self.style.configure("Danger.TButton", font=("Segoe UI", 9, "bold"))
        self.style.configure("Accent.TButton", font=("Segoe UI", 9, "bold"))
        self.style.configure("Treeview", rowheight=24, font=("Segoe UI", 9))
        self.style.configure("Treeview.Heading", font=("Segoe UI", 9, "bold"))

    def _init_variables(self) -> None:
        self.port_var = tk.StringVar(value="")
        self.baud_var = tk.StringVar(value="9600")
        self.timeout_var = tk.StringVar(value=str(DEFAULT_TIMEOUT_MS))
        self.retries_var = tk.StringVar(value=str(DEFAULT_RETRIES))
        self.post_delay_var = tk.StringVar(value=str(DEFAULT_POST_DELAY_MS))
        self.scan_gap_var = tk.StringVar(value=str(DEFAULT_SCAN_GAP_MS))
        self.status_var = tk.StringVar(value="Disconnected")

        labels = self.registry.labels()
        default_profile = labels[0] if labels else ""
        self.current_profile_var = tk.StringVar(value=default_profile)
        self._profile_syncing = False
        self._address_syncing = False

        self.scan_profile_var = tk.StringVar(value=default_profile)
        self.scan_all_profiles_var = tk.BooleanVar(value=False)
        self.scan_start_addr_var = tk.StringVar(value="1")
        self.scan_start_addr_hex_var = tk.StringVar(value="0x01")
        self.scan_end_addr_var = tk.StringVar(value="247")
        self.scan_end_addr_hex_var = tk.StringVar(value="0xF7")

        self.read_profile_var = tk.StringVar(value=default_profile)
        self.read_addr_var = tk.StringVar(value="1")
        self.read_addr_hex_var = tk.StringVar(value="0x01")
        self.cont_read_var = tk.BooleanVar(value=False)
        self.read_interval_var = tk.StringVar(value="1000")

        self.change_profile_var = tk.StringVar(value=default_profile)
        self.change_current_addr_var = tk.StringVar(value="1")
        self.change_current_addr_hex_var = tk.StringVar(value="0x01")
        self.change_new_addr_var = tk.StringVar(value="2")
        self.change_new_addr_hex_var = tk.StringVar(value="0x02")
        self.confirm_single_device_var = tk.BooleanVar(value=False)
        self.confirm_power_var = tk.BooleanVar(value=False)
        self.confirm_risk_var = tk.BooleanVar(value=False)
        self.confirm_wire_var = tk.BooleanVar(value=False)

        self.raw_hex_var = tk.StringVar(value="")
        self.raw_auto_crc_var = tk.BooleanVar(value=True)
        self.raw_tx_preview_var = tk.StringVar(value="TX preview: empty")
        self.action_status_var = tk.StringVar(value="Ready")
        self.scan_status_var = tk.StringVar(value="Scan idle")
        self.scan_verbose_var = tk.BooleanVar(value=True)
        self.read_status_var = tk.StringVar(value="Read idle")
        self.change_status_var = tk.StringVar(value="Address change idle")
        self.raw_status_var = tk.StringVar(value="Raw Modbus idle")

        self.epever_addr_var = tk.StringVar(value="1")
        self.epever_addr_hex_var = tk.StringVar(value="0x01")
        self.epever_target_addr_var = tk.StringVar(value="96")
        self.epever_target_addr_hex_var = tk.StringVar(value="0x60")
        self.epever_preset_var = tk.StringVar(value="PKCELL 2x 12V9Ah parallel")
        self.epever_capacity_var = tk.StringVar(value="18")
        self.epever_max_charge_current_var = tk.StringVar(value="3.60")
        self.epever_rated_voltage_level_var = tk.StringVar(value="1")
        self.epever_confirm_single_var = tk.BooleanVar(value=False)
        self.epever_confirm_battery_var = tk.BooleanVar(value=False)
        self.epever_confirm_backup_var = tk.BooleanVar(value=False)
        self.epever_confirm_risk_var = tk.BooleanVar(value=False)
        self.epever_confirm_experimental_id_var = tk.BooleanVar(value=False)
        self.epever_status_var = tk.StringVar(value="EPEVER config idle")
        self.epever_voltage_vars: Dict[str, tk.StringVar] = {}
        for reg, _label, key in EPEVER_VOLTAGE_ORDER:
            self.epever_voltage_vars[key] = tk.StringVar(value=str(EPEVER_PRESETS[self.epever_preset_var.get()]["voltage_block"][key]))

        self._setup_address_pair(self.scan_start_addr_var, self.scan_start_addr_hex_var)
        self._setup_address_pair(self.scan_end_addr_var, self.scan_end_addr_hex_var)
        self._setup_address_pair(self.read_addr_var, self.read_addr_hex_var)
        self._setup_address_pair(self.change_current_addr_var, self.change_current_addr_hex_var, on_change=self.update_change_preview)
        self._setup_address_pair(self.change_new_addr_var, self.change_new_addr_hex_var, on_change=self.update_change_preview)
        self._setup_address_pair(self.epever_addr_var, self.epever_addr_hex_var)
        self._setup_address_pair(self.epever_target_addr_var, self.epever_target_addr_hex_var)

    def _setup_address_pair(self, dec_var: tk.StringVar, hex_var: tk.StringVar, on_change=None) -> None:
        def dec_changed(*_args) -> None:
            if self._address_syncing:
                return
            try:
                addr = parse_address(dec_var.get())
            except Exception:
                return
            self._address_syncing = True
            try:
                hex_var.set(f"0x{addr:02X}")
            finally:
                self._address_syncing = False
            if on_change:
                try:
                    on_change()
                except Exception:
                    pass

        def hex_changed(*_args) -> None:
            if self._address_syncing:
                return
            try:
                addr = parse_address(hex_var.get())
            except Exception:
                return
            self._address_syncing = True
            try:
                dec_var.set(str(addr))
            finally:
                self._address_syncing = False
            if on_change:
                try:
                    on_change()
                except Exception:
                    pass

        dec_var.trace_add("write", dec_changed)
        hex_var.trace_add("write", hex_changed)

    def _build_ui(self) -> None:
        self._build_top_connection_bar()
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill="both", expand=True, padx=8, pady=8)

        self.connection_tab = ttk.Frame(self.notebook)
        self.scan_tab = ttk.Frame(self.notebook)
        self.read_tab = ttk.Frame(self.notebook)
        self.change_tab = ttk.Frame(self.notebook)
        self.raw_tab = ttk.Frame(self.notebook)
        self.epever_tab = ttk.Frame(self.notebook)
        self.logs_tab = ttk.Frame(self.notebook)

        self.notebook.add(self.connection_tab, text="Connection")
        self.notebook.add(self.scan_tab, text="Scan")
        self.notebook.add(self.read_tab, text="Read Device")
        self.notebook.add(self.change_tab, text="Change Address")
        self.notebook.add(self.raw_tab, text="Raw Modbus")
        self.notebook.add(self.epever_tab, text="EPEVER Config")
        self.notebook.add(self.logs_tab, text="Logs")

        self._build_connection_tab()
        self._build_scan_tab()
        self._build_read_tab()
        self._build_change_tab()
        self._build_raw_tab()
        self._build_epever_tab()
        self._build_logs_tab()
        self.update_change_confirmations()
        self.update_change_preview()
        self.update_raw_crc_preview()

    def _build_top_connection_bar(self) -> None:
        bar = ttk.Frame(self, style="Top.TFrame", padding=(10, 8))
        bar.pack(fill="x")

        ttk.Label(bar, text="USB-RS485 Sensor Tool", style="Status.TLabel").pack(side="left", padx=(0, 16))

        ttk.Label(bar, text="Port:", style="Status.TLabel").pack(side="left")
        self.port_combo = ttk.Combobox(bar, textvariable=self.port_var, width=44, values=[])
        self.port_combo.pack(side="left", padx=(4, 8))
        ttk.Button(bar, text="Refresh ports", command=self.refresh_ports).pack(side="left", padx=(0, 10))

        ttk.Label(bar, text="Baud:", style="Status.TLabel").pack(side="left")
        self.baud_combo = ttk.Combobox(bar, textvariable=self.baud_var, width=9, values=[str(x) for x in BAUD_RATES], state="readonly")
        self.baud_combo.pack(side="left", padx=(4, 8))

        ttk.Label(bar, text="Timeout:", style="Status.TLabel").pack(side="left")
        ttk.Entry(bar, textvariable=self.timeout_var, width=7).pack(side="left", padx=(4, 4))
        ttk.Label(bar, text="ms", style="Status.TLabel").pack(side="left", padx=(0, 8))

        ttk.Label(bar, text="Retries:", style="Status.TLabel").pack(side="left")
        ttk.Entry(bar, textvariable=self.retries_var, width=5).pack(side="left", padx=(4, 8))

        ttk.Button(bar, text="Connect", style="Accent.TButton", command=self.connect_clicked).pack(side="left", padx=(8, 4))
        ttk.Button(bar, text="Disconnect", command=self.disconnect_clicked).pack(side="left", padx=(0, 12))

        ttk.Label(bar, textvariable=self.status_var, style="Status.TLabel").pack(side="left", padx=(8, 14))
        ttk.Label(bar, text="|", style="Status.TLabel").pack(side="left")
        ttk.Label(bar, textvariable=self.action_status_var, style="Status.TLabel").pack(side="left", padx=(12, 8))
        self.busy_progress = ttk.Progressbar(bar, mode="determinate", length=150, maximum=100)
        self.busy_progress.pack(side="left", padx=(8, 0))

    def _build_connection_tab(self) -> None:
        frame = ttk.Frame(self.connection_tab, padding=18)
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="USB-RS485 Sensor Tool", style="Title.TLabel").pack(anchor="w")
        ttk.Label(
            frame,
            text="Standalone utility for scanning, reading and safely configuring RS485/Modbus sensors through a USB-to-RS485 adapter.",
            style="Hint.TLabel",
            justify="left",
        ).pack(anchor="w", pady=(4, 14))

        quick = ttk.LabelFrame(frame, text="Recommended workflow", style="Card.TLabelframe", padding=12)
        quick.pack(fill="x", pady=(0, 12))
        ttk.Label(
            quick,
            text=(
                "1) Connect the USB-to-RS485 adapter and select COM port.\n"
                "2) Select the sensor profile. The app can set the profile baud automatically from the profile.\n"
                "3) Use Scan to find the address, then Read Device to verify values.\n"
                "4) Use Change Address only when exactly one target sensor is connected."
            ),
            style="Info.TLabel",
            justify="left",
        ).pack(anchor="w")

        settings = ttk.LabelFrame(frame, text="Timing settings", style="Card.TLabelframe", padding=12)
        settings.pack(fill="x", pady=12)
        ttk.Label(settings, text="Post-request delay ms:").grid(row=0, column=0, sticky="w", padx=4, pady=4)
        ttk.Entry(settings, textvariable=self.post_delay_var, width=10).grid(row=0, column=1, sticky="w", padx=4, pady=4)
        ttk.Label(settings, text="Scan gap ms:").grid(row=0, column=2, sticky="w", padx=20, pady=4)
        ttk.Entry(settings, textvariable=self.scan_gap_var, width=10).grid(row=0, column=3, sticky="w", padx=4, pady=4)
        ttk.Label(settings, text="Defaults: 9600 8N1, timeout 1500 ms, retries 3.", style="Hint.TLabel").grid(row=1, column=0, columnspan=4, sticky="w", padx=4, pady=(8, 0))

        driver_note = ttk.LabelFrame(frame, text="Driver / wiring notes", style="Card.TLabelframe", padding=12)
        driver_note.pack(fill="x", pady=8)
        ttk.Label(
            driver_note,
            text=(
                "If no COM ports appear, check USB cable, USB-to-RS485 driver and whether another serial monitor is using the port.\n"
                "Common chips: CH340/CH343, CP210x, FT232. A/B line swap is a very common reason for zero response."
            ),
            style="Info.TLabel",
            justify="left",
        ).pack(anchor="w")

        safety = ttk.LabelFrame(frame, text="Safety rule", style="Card.TLabelframe", padding=12)
        safety.pack(fill="x", pady=8)
        ttk.Label(
            safety,
            text="Scan never sends write/change/reset commands. Address change and reset operations are separated and guarded with confirmations.",
            style="Info.TLabel",
            justify="left",
        ).pack(anchor="w")

    def _build_scan_tab(self) -> None:
        controls = ttk.LabelFrame(self.scan_tab, text="Scan options", style="Card.TLabelframe", padding=12)
        controls.pack(fill="x", padx=8, pady=8)

        labels_all = self.registry.labels()
        ttk.Checkbutton(controls, text="All supported profiles", variable=self.scan_all_profiles_var).grid(row=0, column=0, sticky="w", padx=4, pady=4)
        ttk.Label(controls, text="Profile:").grid(row=0, column=1, sticky="e", padx=4, pady=4)
        self.scan_profile_combo = ttk.Combobox(controls, textvariable=self.scan_profile_var, values=labels_all, width=42, state="readonly")
        self.scan_profile_combo.grid(row=0, column=2, sticky="w", padx=4, pady=4)
        self.scan_profile_combo.bind("<<ComboboxSelected>>", lambda _e: self.on_profile_selected("scan"))
        ttk.Label(controls, text="Start dec:").grid(row=0, column=3, sticky="e", padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.scan_start_addr_var, width=7).grid(row=0, column=4, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="hex:").grid(row=0, column=5, sticky="e", padx=2, pady=4)
        ttk.Entry(controls, textvariable=self.scan_start_addr_hex_var, width=8).grid(row=0, column=6, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="End dec:").grid(row=0, column=7, sticky="e", padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.scan_end_addr_var, width=7).grid(row=0, column=8, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="hex:").grid(row=0, column=9, sticky="e", padx=2, pady=4)
        ttk.Entry(controls, textvariable=self.scan_end_addr_hex_var, width=8).grid(row=0, column=10, sticky="w", padx=2, pady=4)
        self.scan_button = ttk.Button(controls, text="Start scan", style="Accent.TButton", command=self.start_scan)
        self.scan_button.grid(row=1, column=0, padx=4, pady=(8, 4), sticky="w")
        ttk.Button(controls, text="Stop", command=self.stop_scan).grid(row=1, column=1, padx=4, pady=(8, 4), sticky="w")
        ttk.Button(controls, text="Clear", command=self.clear_scan_results).grid(row=1, column=2, padx=4, pady=(8, 4), sticky="w")
        ttk.Button(controls, text="Export CSV", command=self.export_scan_csv).grid(row=1, column=3, padx=4, pady=(8, 4), sticky="w")
        ttk.Button(controls, text="Export JSON", command=self.export_scan_json).grid(row=1, column=4, padx=4, pady=(8, 4), sticky="w")
        ttk.Button(controls, text="Use profile baud", command=lambda: self.apply_profile_baud(self.scan_profile_var.get(), "scan")).grid(row=1, column=5, padx=4, pady=(8, 4), sticky="w")
        ttk.Checkbutton(controls, text="Verbose TX/RX log", variable=self.scan_verbose_var).grid(row=1, column=6, padx=4, pady=(8, 4), sticky="w")
        ttk.Label(controls, textvariable=self.scan_status_var, style="Hint.TLabel").grid(row=2, column=0, columnspan=11, sticky="w", padx=4, pady=(8, 0))

        result_frame = ttk.Frame(self.scan_tab)
        result_frame.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        columns = ("time", "baud", "address", "profile", "quick", "elapsed", "crc", "rx")
        self.scan_tree = ttk.Treeview(result_frame, columns=columns, show="headings", height=18)
        headings = {
            "time": "Time",
            "baud": "Baud",
            "address": "Address",
            "profile": "Profile",
            "quick": "Quick parsed value",
            "elapsed": "ms",
            "crc": "CRC/Status",
            "rx": "RX raw",
        }
        widths = {"time": 155, "baud": 70, "address": 90, "profile": 260, "quick": 260, "elapsed": 70, "crc": 130, "rx": 520}
        for col in columns:
            self.scan_tree.heading(col, text=headings[col])
            self.scan_tree.column(col, width=widths[col], anchor="w")
        yscroll = ttk.Scrollbar(result_frame, orient="vertical", command=self.scan_tree.yview)
        self.scan_tree.configure(yscrollcommand=yscroll.set)
        self.scan_tree.pack(side="left", fill="both", expand=True)
        yscroll.pack(side="right", fill="y")

        scan_log_frame = ttk.LabelFrame(self.scan_tab, text="Live scan TX/RX log", style="Card.TLabelframe", padding=8)
        scan_log_frame.pack(fill="both", expand=False, padx=8, pady=(0, 8))
        scan_log_buttons = ttk.Frame(scan_log_frame)
        scan_log_buttons.pack(fill="x", pady=(0, 4))
        ttk.Button(scan_log_buttons, text="Copy scan log", command=lambda: self.copy_text_widget_all(self.scan_live_text)).pack(side="left", padx=4)
        ttk.Button(scan_log_buttons, text="Clear scan log", command=lambda: self.scan_live_text.delete("1.0", "end")).pack(side="left", padx=4)
        self.scan_live_text = ScrolledText(scan_log_frame, height=8, wrap="word", font=("Consolas", 9), background="#ffffff", foreground="#111827")
        self.scan_live_text.pack(fill="both", expand=True)

    def _build_read_tab(self) -> None:
        controls = ttk.LabelFrame(self.read_tab, text="Read device", style="Card.TLabelframe", padding=12)
        controls.pack(fill="x", padx=10, pady=10)

        ttk.Label(controls, text="Profile:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        self.read_profile_combo = ttk.Combobox(controls, textvariable=self.read_profile_var, values=self.registry.labels(), width=48, state="readonly")
        self.read_profile_combo.grid(row=0, column=1, sticky="w", padx=4, pady=4)
        self.read_profile_combo.bind("<<ComboboxSelected>>", lambda _e: self.on_profile_selected("read"))

        ttk.Label(controls, text="Address dec:").grid(row=0, column=2, sticky="e", padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.read_addr_var, width=7).grid(row=0, column=3, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="hex:").grid(row=0, column=4, sticky="e", padx=2, pady=4)
        ttk.Entry(controls, textvariable=self.read_addr_hex_var, width=8).grid(row=0, column=5, sticky="w", padx=2, pady=4)
        self.read_once_button = ttk.Button(controls, text="Send read request", style="Accent.TButton", command=self.read_once_clicked)
        self.read_once_button.grid(row=0, column=6, padx=8, pady=4)
        ttk.Checkbutton(controls, text="Continuous", variable=self.cont_read_var, command=self.continuous_read_toggled).grid(row=0, column=7, padx=8, pady=4)
        ttk.Label(controls, text="Interval ms:").grid(row=0, column=8, sticky="e", padx=4, pady=4)
        ttk.Entry(controls, textvariable=self.read_interval_var, width=8).grid(row=0, column=9, sticky="w", padx=4, pady=4)
        ttk.Button(controls, text="Stop", command=self.stop_continuous_read).grid(row=0, column=10, padx=8, pady=4)
        ttk.Button(controls, text="Use profile baud", command=lambda: self.apply_profile_baud(self.read_profile_var.get(), "read")).grid(row=0, column=11, padx=4, pady=4)
        ttk.Label(controls, textvariable=self.read_status_var, style="Hint.TLabel").grid(row=1, column=0, columnspan=12, sticky="w", padx=4, pady=(8, 0))

        main = ttk.PanedWindow(self.read_tab, orient="vertical")
        main.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        values_frame = ttk.LabelFrame(main, text="Parsed values", style="Card.TLabelframe", padding=8)
        raw_frame = ttk.LabelFrame(main, text="Raw transaction / request status", style="Card.TLabelframe", padding=8)
        main.add(values_frame, weight=3)
        main.add(raw_frame, weight=2)

        columns = ("field", "value", "unit", "raw", "status")
        self.read_tree = ttk.Treeview(values_frame, columns=columns, show="headings", height=10)
        for col, title, width in [
            ("field", "Field", 280),
            ("value", "Value", 170),
            ("unit", "Unit", 90),
            ("raw", "Raw", 130),
            ("status", "Status", 260),
        ]:
            self.read_tree.heading(col, text=title)
            self.read_tree.column(col, width=width, anchor="w")
        self.read_tree.pack(fill="both", expand=True)

        raw_buttons = ttk.Frame(raw_frame)
        raw_buttons.pack(fill="x", pady=(0, 6))
        ttk.Button(raw_buttons, text="Copy raw details", command=lambda: self.copy_text_widget_all(self.read_raw_text)).pack(side="left", padx=4)
        ttk.Button(raw_buttons, text="Clear raw details", command=lambda: self.read_raw_text.delete("1.0", "end")).pack(side="left", padx=4)

        self.read_raw_text = ScrolledText(raw_frame, height=10, wrap="word", font=("Consolas", 10), background="#ffffff", foreground="#111827")
        self.read_raw_text.pack(fill="both", expand=True)

    def _build_change_tab(self) -> None:
        root = ttk.Frame(self.change_tab, padding=12)
        root.pack(fill="both", expand=True)

        controls = ttk.LabelFrame(root, text="Address change wizard", style="Card.TLabelframe", padding=12)
        controls.pack(fill="x")

        ttk.Label(controls, text="Profile:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        combo = ttk.Combobox(controls, textvariable=self.change_profile_var, values=self.registry.labels(), width=48, state="readonly")
        combo.grid(row=0, column=1, sticky="w", padx=4, pady=4)
        combo.bind("<<ComboboxSelected>>", lambda _e: self.on_profile_selected("change"))

        ttk.Label(controls, text="Current dec:").grid(row=0, column=2, sticky="e", padx=4, pady=4)
        cur_entry = ttk.Entry(controls, textvariable=self.change_current_addr_var, width=7)
        cur_entry.grid(row=0, column=3, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="hex:").grid(row=0, column=4, sticky="e", padx=2, pady=4)
        cur_hex_entry = ttk.Entry(controls, textvariable=self.change_current_addr_hex_var, width=8)
        cur_hex_entry.grid(row=0, column=5, sticky="w", padx=2, pady=4)

        ttk.Label(controls, text="New dec:").grid(row=0, column=6, sticky="e", padx=4, pady=4)
        new_entry = ttk.Entry(controls, textvariable=self.change_new_addr_var, width=7)
        new_entry.grid(row=0, column=7, sticky="w", padx=2, pady=4)
        ttk.Label(controls, text="hex:").grid(row=0, column=8, sticky="e", padx=2, pady=4)
        new_hex_entry = ttk.Entry(controls, textvariable=self.change_new_addr_hex_var, width=8)
        new_hex_entry.grid(row=0, column=9, sticky="w", padx=2, pady=4)

        ttk.Button(controls, text="Update preview", command=self.update_change_preview).grid(row=1, column=0, padx=4, pady=(8, 4), sticky="w")
        self.change_address_button = ttk.Button(controls, text="Send change-address command", style="Danger.TButton", command=self.change_address_clicked)
        self.change_address_button.grid(row=1, column=1, padx=8, pady=(8, 4), sticky="w")
        ttk.Label(controls, textvariable=self.change_status_var, style="Hint.TLabel").grid(row=2, column=0, columnspan=10, sticky="w", padx=4, pady=(8, 0))

        warning = ttk.LabelFrame(root, text="Required confirmations", style="Card.TLabelframe", padding=12)
        warning.pack(fill="x", pady=10)
        ttk.Label(
            warning,
            text="These checkboxes must be intentionally enabled before the app sends any address-change command.",
            style="Hint.TLabel",
        ).pack(anchor="w", pady=(0, 6))
        self.confirm_single_device_check = ttk.Checkbutton(warning, text="✓ Only one target device is connected to the RS485 line", variable=self.confirm_single_device_var)
        self.confirm_single_device_check.pack(anchor="w", pady=2)
        self.confirm_power_check = ttk.Checkbutton(warning, text="✓ Power wiring is stable and A/B lines are connected correctly", variable=self.confirm_power_var)
        self.confirm_power_check.pack(anchor="w", pady=2)
        self.confirm_risk_check = ttk.Checkbutton(warning, text="✓ I understand that wrong address change can reconfigure the sensor", variable=self.confirm_risk_var)
        self.confirm_risk_check.pack(anchor="w", pady=2)
        self.confirm_wire_check = ttk.Checkbutton(warning, text="✓ White/config wire is connected exactly as this profile requires", variable=self.confirm_wire_var)
        self.confirm_wire_check.pack(anchor="w", pady=2)
        self.confirm_profile_note_var = tk.StringVar(value="")
        ttk.Label(warning, textvariable=self.confirm_profile_note_var, style="Hint.TLabel", wraplength=1200).pack(anchor="w", pady=(8, 0))

        preview_frame = ttk.LabelFrame(root, text="Command preview and result", style="Card.TLabelframe", padding=8)
        preview_frame.pack(fill="both", expand=True)
        preview_buttons = ttk.Frame(preview_frame)
        preview_buttons.pack(fill="x", pady=(0, 6))
        ttk.Button(preview_buttons, text="Copy preview/result", command=lambda: self.copy_text_widget_all(self.change_preview_text)).pack(side="left", padx=4)
        ttk.Button(preview_buttons, text="Clear result", command=lambda: (self.update_change_preview(), self.change_status_var.set("Address change idle"))).pack(side="left", padx=4)
        self.change_preview_text = ScrolledText(preview_frame, wrap="word", font=("Consolas", 10), background="#ffffff", foreground="#111827")
        self.change_preview_text.pack(fill="both", expand=True)

    def _build_raw_tab(self) -> None:
        controls = ttk.LabelFrame(self.raw_tab, text="Raw Modbus request", style="Card.TLabelframe", padding=12)
        controls.pack(fill="x", padx=10, pady=10)

        ttk.Label(controls, text="HEX request:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        raw_entry = ttk.Entry(controls, textvariable=self.raw_hex_var, width=80)
        raw_entry.grid(row=0, column=1, sticky="we", padx=4, pady=4)
        controls.grid_columnconfigure(1, weight=1)
        ttk.Checkbutton(
            controls,
            text="Auto-calculate and append CRC (input WITHOUT CRC)",
            variable=self.raw_auto_crc_var,
            command=self.update_raw_crc_preview,
        ).grid(row=0, column=2, padx=8, pady=4)
        self.raw_send_button = ttk.Button(controls, text="Send HEX request", style="Accent.TButton", command=self.raw_send_clicked)
        self.raw_send_button.grid(row=0, column=3, padx=4, pady=4)
        ttk.Button(controls, text="Calculate CRC preview", command=self.update_raw_crc_preview).grid(row=0, column=4, padx=4, pady=4)
        ttk.Button(controls, text="Copy TX full", command=self.copy_raw_tx_preview).grid(row=0, column=5, padx=4, pady=4)
        ttk.Button(controls, text="Clear", command=lambda: (self.raw_text.delete("1.0", "end"), self.raw_status_var.set("Raw Modbus idle"))).grid(row=0, column=6, padx=4, pady=4)

        ttk.Label(controls, textvariable=self.raw_tx_preview_var, style="Hint.TLabel").grid(row=1, column=0, columnspan=7, sticky="w", padx=4, pady=(8, 0))
        ttk.Label(controls, textvariable=self.raw_status_var, style="Hint.TLabel").grid(row=2, column=0, columnspan=7, sticky="w", padx=4, pady=(4, 0))

        self.raw_hex_var.trace_add("write", lambda *_args: self.update_raw_crc_preview())

        examples = ttk.LabelFrame(self.raw_tab, text="Examples without CRC", style="Card.TLabelframe", padding=8)
        examples.pack(fill="x", padx=10, pady=(0, 8))
        btns = ttk.Frame(examples)
        btns.pack(fill="x")
        for text, cmd in [
            ("Honde WS5 probe", "10 03 00 0B 00 02"),
            ("Honde Leaf read", "20 03 00 00 00 02"),
            ("Honde Air T/H read", "01 03 00 00 00 02"),
            ("Honde Air T/H change addr 1→2", "FE 06 0A 00 00 02"),
            ("Solar radiation read", "30 03 00 00 00 01"),
            ("Lumiax battery voltage", "60 04 30 46 00 01"),
            ("TUF flow + velocity", "01 03 00 00 00 06"),
            ("TUF error bits", "01 03 00 47 00 01"),
            ("TUF M90 diagnostics", "01 03 00 5B 00 03"),
            ("TUF M91 time ratio", "01 03 00 60 00 02"),
            ("TUF inner diameter", "01 03 00 DC 00 02"),
            ("TUF configured ID", "01 03 05 A1 00 01"),
            ("EPEVER battery voltage", "01 04 31 08 00 01"),
            ("EPEVER read battery params", "01 03 90 00 00 03"),
            ("EPEVER write max current 3.60A", "01 10 90 BF 00 01 02 01 68"),
            ("EPEVER custom find ID", "F8 45 00 01 01 F8"),
            ("EPEVER custom set ID 0x60", "F8 45 00 01 01 60"),
        ]:
            index = len(btns.grid_slaves())
            ttk.Button(
                btns,
                text=text,
                command=lambda c=cmd: self.set_raw_example(c),
            ).grid(row=index // 4, column=index % 4, sticky="w", padx=4, pady=2)

        self.raw_text = ScrolledText(self.raw_tab, wrap="word", font=("Consolas", 10), background="#ffffff", foreground="#111827")
        self.raw_text.pack(fill="both", expand=True, padx=10, pady=(0, 10))

    def _build_epever_tab(self) -> None:
        # Split the EPEVER page into a scrollable settings pane and a permanently
        # visible TX/RX pane. v0.7 could collapse the log area on smaller screens,
        # which was visually hilarious and diagnostically useless.
        root = ttk.PanedWindow(self.epever_tab, orient=tk.HORIZONTAL)
        root.pack(fill="both", expand=True, padx=8, pady=8)

        left_shell = ttk.Frame(root)
        right_shell = ttk.Frame(root, padding=(8, 0, 0, 0))
        root.add(left_shell, weight=3)
        root.add(right_shell, weight=2)

        canvas = tk.Canvas(left_shell, highlightthickness=0, background="#f7f9fc")
        yscroll = ttk.Scrollbar(left_shell, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=yscroll.set)
        yscroll.pack(side="right", fill="y")
        canvas.pack(side="left", fill="both", expand=True)

        controls = ttk.Frame(canvas, padding=10)
        canvas_window = canvas.create_window((0, 0), window=controls, anchor="nw")

        def _update_scroll_region(_event=None):
            canvas.configure(scrollregion=canvas.bbox("all"))

        def _fit_inner_width(event):
            canvas.itemconfigure(canvas_window, width=event.width)

        controls.bind("<Configure>", _update_scroll_region)
        canvas.bind("<Configure>", _fit_inner_width)

        intro = ttk.LabelFrame(controls, text="EPEVER / Tracer-AN G3 configuration", style="Card.TLabelframe", padding=12)
        intro.pack(fill="x")
        ttk.Label(
            intro,
            text=(
                "For Tracer-AN G3 / XTRA-N G3 controllers. Uses Modbus RTU 115200 8N1 and FC10 writes. "
                "Voltage block 0x9003..0x900E is written as one group. EPEVER ID change uses captured custom service frame 0x45."
            ),
            style="Hint.TLabel",
            wraplength=820,
            justify="left",
        ).pack(anchor="w")

        top = ttk.LabelFrame(controls, text="Connection and preset", style="Card.TLabelframe", padding=12)
        top.pack(fill="x", pady=(10, 8))
        ttk.Label(top, text="Current address dec:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        ttk.Entry(top, textvariable=self.epever_addr_var, width=7).grid(row=0, column=1, sticky="w", padx=2, pady=4)
        ttk.Label(top, text="hex:").grid(row=0, column=2, sticky="e", padx=2, pady=4)
        ttk.Entry(top, textvariable=self.epever_addr_hex_var, width=8).grid(row=0, column=3, sticky="w", padx=2, pady=4)
        ttk.Button(top, text="Use EPEVER baud 115200", command=lambda: self.baud_var.set("115200")).grid(row=0, column=4, padx=10, pady=4)
        ttk.Label(top, text="Preset:").grid(row=1, column=0, sticky="e", padx=4, pady=4)
        preset_combo = ttk.Combobox(top, textvariable=self.epever_preset_var, values=list(EPEVER_PRESETS.keys()), width=34, state="readonly")
        preset_combo.grid(row=1, column=1, columnspan=3, sticky="w", padx=4, pady=4)
        preset_combo.bind("<<ComboboxSelected>>", lambda _e: self.load_epever_preset())
        ttk.Button(top, text="Load preset values", command=self.load_epever_preset).grid(row=1, column=4, padx=10, pady=4)
        ttk.Label(top, text="Target/new address:").grid(row=2, column=0, sticky="e", padx=4, pady=4)
        ttk.Entry(top, textvariable=self.epever_target_addr_var, width=7).grid(row=2, column=1, sticky="w", padx=2, pady=4)
        ttk.Label(top, text="hex:").grid(row=2, column=2, sticky="e", padx=2, pady=4)
        ttk.Entry(top, textvariable=self.epever_target_addr_hex_var, width=8).grid(row=2, column=3, sticky="w", padx=2, pady=4)
        ttk.Label(top, text="config.h target: 0x60 / 96", style="Hint.TLabel").grid(row=2, column=4, sticky="w", padx=10, pady=4)

        basic = ttk.LabelFrame(controls, text="Battery profile settings", style="Card.TLabelframe", padding=12)
        basic.pack(fill="x", pady=8)
        ttk.Label(basic, text="Battery type:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
        ttk.Label(basic, text="User mode = 0 (fixed)", style="Hint.TLabel").grid(row=0, column=1, sticky="w", padx=4, pady=4)
        ttk.Label(basic, text="Capacity Ah:").grid(row=0, column=2, sticky="e", padx=4, pady=4)
        ttk.Entry(basic, textvariable=self.epever_capacity_var, width=8).grid(row=0, column=3, sticky="w", padx=4, pady=4)
        ttk.Label(basic, text="Rated voltage level:").grid(row=0, column=4, sticky="e", padx=4, pady=4)
        ttk.Entry(basic, textvariable=self.epever_rated_voltage_level_var, width=8).grid(row=0, column=5, sticky="w", padx=4, pady=4)
        ttk.Label(basic, text="1=12V, 2=24V, 3=36V, 4=48V, 0=auto", style="Hint.TLabel").grid(row=0, column=6, sticky="w", padx=4, pady=4)
        ttk.Label(basic, text="Max charge current A:").grid(row=1, column=0, sticky="e", padx=4, pady=4)
        ttk.Entry(basic, textvariable=self.epever_max_charge_current_var, width=8).grid(row=1, column=1, sticky="w", padx=4, pady=4)
        ttk.Label(basic, text="config.h value: 3.60A → raw 360 at register 0x90BF", style="Hint.TLabel").grid(row=1, column=2, columnspan=5, sticky="w", padx=4, pady=4)

        volt = ttk.LabelFrame(controls, text="Voltage block 0x9003..0x900E, volts", style="Card.TLabelframe", padding=12)
        volt.pack(fill="x", pady=8)
        for idx, (reg, label, key) in enumerate(EPEVER_VOLTAGE_ORDER):
            row = idx // 3
            col = (idx % 3) * 3
            ttk.Label(volt, text=f"0x{reg} {label}:").grid(row=row, column=col, sticky="e", padx=4, pady=4)
            ttk.Entry(volt, textvariable=self.epever_voltage_vars[key], width=8).grid(row=row, column=col + 1, sticky="w", padx=4, pady=4)
            ttk.Label(volt, text="V", style="Hint.TLabel").grid(row=row, column=col + 2, sticky="w")

        hint = ttk.LabelFrame(controls, text="Voltage sanity notes", style="Card.TLabelframe", padding=10)
        hint.pack(fill="x", pady=8)
        ttk.Label(
            hint,
            text=(
                "900A Low Voltage Recovery and 900B Undervoltage Alarm Recovery are separate chains. "
                "The app no longer blocks config.h values where 900B is higher than 900A. It still blocks clearly dangerous high/low voltage ordering."
            ),
            style="Hint.TLabel",
            wraplength=820,
            justify="left",
        ).pack(anchor="w")

        confirm = ttk.LabelFrame(controls, text="Required confirmations before WRITE", style="Card.TLabelframe", padding=12)
        confirm.pack(fill="x", pady=8)
        ttk.Checkbutton(confirm, text="✓ Only this EPEVER controller is connected to this RS485 adapter", variable=self.epever_confirm_single_var).pack(anchor="w", pady=2)
        ttk.Checkbutton(confirm, text="✓ Battery type and voltage values are correct for the connected battery", variable=self.epever_confirm_battery_var).pack(anchor="w", pady=2)
        ttk.Checkbutton(confirm, text="✓ Current controller settings were read/backed up or intentionally ignored", variable=self.epever_confirm_backup_var).pack(anchor="w", pady=2)
        ttk.Checkbutton(confirm, text="✓ I understand wrong charge settings can damage batteries, loads, or the controller", variable=self.epever_confirm_risk_var).pack(anchor="w", pady=2)
        ttk.Checkbutton(confirm, text="✓ For EPEVER ID change: I understand this uses a captured custom service command 0x45", variable=self.epever_confirm_experimental_id_var).pack(anchor="w", pady=2)

        actions = ttk.LabelFrame(controls, text="Actions", style="Card.TLabelframe", padding=12)
        actions.pack(fill="x", pady=8)
        self.epever_read_button = ttk.Button(actions, text="Read current EPEVER config", command=self.epever_read_config_clicked)
        self.epever_read_button.grid(row=0, column=0, padx=4, pady=4, sticky="w")
        self.epever_find_id_button = ttk.Button(actions, text="Find/Read EPEVER ID", command=self.epever_find_id_clicked)
        self.epever_find_id_button.grid(row=0, column=1, padx=4, pady=4, sticky="w")
        self.epever_change_id_button = ttk.Button(actions, text="CHANGE EPEVER ID", style="Danger.TButton", command=self.epever_change_id_clicked)
        self.epever_change_id_button.grid(row=0, column=2, padx=4, pady=4, sticky="w")
        self.epever_write_all_button = ttk.Button(actions, text="WRITE selected config", style="Danger.TButton", command=self.epever_write_all_clicked)
        self.epever_write_all_button.grid(row=1, column=0, padx=4, pady=4, sticky="w")
        self.epever_write_voltage_button = ttk.Button(actions, text="Write voltage block only", command=self.epever_write_voltage_clicked)
        self.epever_write_voltage_button.grid(row=1, column=1, padx=4, pady=4, sticky="w")
        self.epever_write_current_button = ttk.Button(actions, text="Write max charge current only", command=self.epever_write_current_clicked)
        self.epever_write_current_button.grid(row=1, column=2, padx=4, pady=4, sticky="w")
        ttk.Label(actions, textvariable=self.epever_status_var, style="Hint.TLabel", wraplength=820).grid(row=2, column=0, columnspan=3, sticky="w", padx=4, pady=(8, 0))

        result = ttk.LabelFrame(right_shell, text="EPEVER TX/RX and result", style="Card.TLabelframe", padding=8)
        result.pack(fill="both", expand=True)
        result_buttons = ttk.Frame(result)
        result_buttons.pack(fill="x", pady=(0, 6))
        ttk.Button(result_buttons, text="Copy all", command=lambda: self.copy_text_widget_all(self.epever_text)).pack(side="left", padx=4)
        ttk.Button(result_buttons, text="Clear", command=lambda: self.epever_text.delete("1.0", "end")).pack(side="left", padx=4)
        ttk.Button(result_buttons, text="Insert separator", command=lambda: self.append_text_widget(self.epever_text, "\n" + "-" * 78 + "\n")).pack(side="left", padx=4)
        self.epever_text = ScrolledText(result, wrap="word", height=32, font=("Consolas", 10), background="#ffffff", foreground="#111827")
        self.epever_text.pack(fill="both", expand=True)
        self.epever_text.insert("end", f"EPEVER TX/RX log ready. App v{APP_VERSION}.\n")
        self.epever_text.insert("end", "Use this pane to see every TX/RX frame, validation warning, and write/read result.\n")
        self.load_epever_preset()

    def _build_logs_tab(self) -> None:
        controls = ttk.Frame(self.logs_tab)
        controls.pack(fill="x", padx=8, pady=8)
        ttk.Button(controls, text="Copy selected", command=self.copy_log_selected).pack(side="left", padx=4)
        ttk.Button(controls, text="Copy all", command=self.copy_log_all).pack(side="left", padx=4)
        ttk.Button(controls, text="Save log", command=self.save_log).pack(side="left", padx=4)
        ttk.Button(controls, text="Clear log", command=lambda: self.log_text.delete("1.0", "end")).pack(side="left", padx=4)

        self.log_text = ScrolledText(self.logs_tab, wrap="word", font=("Consolas", 10))
        self.log_text.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.log(f"{APP_TITLE} started")

    # ------------------------------------------------------------------
    # UI helpers
    # ------------------------------------------------------------------

    def process_ui_queue(self) -> None:
        # Do not drain an unlimited queue in one UI tick. During a scan the worker
        # can generate many TX/RX log events; processing them all at once makes
        # Tkinter look frozen even though the worker is separate. Naturally, UI
        # toolkits punish enthusiasm.
        processed = 0
        try:
            while processed < 80:
                func, args = self.ui_queue.get_nowait()
                func(*args)
                processed += 1
        except queue.Empty:
            pass
        self.after(30, self.process_ui_queue)

    def call_ui(self, func, *args) -> None:
        self.ui_queue.put((func, args))

    def log(self, message: str) -> None:
        if not hasattr(self, "log_text"):
            return
        self.log_text.insert("end", f"[{now_log_time()}] {message}\n")
        self.log_text.see("end")

    def log_block(self, title: str, lines: List[str]) -> None:
        sep = "-" * 90
        self.log_text.insert("end", f"{sep}\n{now_log_time()}  {title}\n{sep}\n")
        for line in lines:
            self.log_text.insert("end", f"{line}\n")
        self.log_text.see("end")

    def show_error(self, title: str, message: str) -> None:
        self.log(f"ERROR: {title}: {message}")
        messagebox.showerror(title, message)

    def get_timeout_ms(self) -> int:
        return max(10, int(self.timeout_var.get().strip()))

    def get_retries(self) -> int:
        return max(1, int(self.retries_var.get().strip()))

    def get_post_delay_ms(self) -> int:
        return max(0, int(self.post_delay_var.get().strip()))

    def get_scan_gap_ms(self) -> int:
        return max(0, int(self.scan_gap_var.get().strip()))

    def selected_port(self) -> str:
        return self.port_var.get().strip()

    def selected_baud(self) -> int:
        return int(self.baud_var.get().strip())

    def on_profile_selected(self, source: str) -> None:
        if source == "scan":
            label = self.scan_profile_var.get()
        elif source == "read":
            label = self.read_profile_var.get()
        elif source == "change":
            label = self.change_profile_var.get()
        else:
            label = self.current_profile_var.get()
        self.set_profile_everywhere(label, source)
        self.apply_profile_baud(label, source)
        if source == "change":
            self.update_change_confirmations()
            self.update_change_preview()

    def set_profile_everywhere(self, label: str, source: str = "profile") -> None:
        if self._profile_syncing or not label:
            return
        self._profile_syncing = True
        try:
            self.current_profile_var.set(label)
            self.scan_profile_var.set(label)
            self.read_profile_var.set(label)
            self.change_profile_var.set(label)
        finally:
            self._profile_syncing = False
        self.action_status_var.set(f"Profile selected: {label}")

    def set_action_status(self, message: str) -> None:
        self.action_status_var.set(message)

    def set_busy_indicator(self, busy: bool) -> None:
        # Determinate progress is intentionally used instead of the vague
        # indeterminate bouncing block. The old animation looked like the app
        # was alive while giving zero information, a very Windows-flavored insult.
        if hasattr(self, "busy_progress"):
            if busy:
                self.busy_progress.configure(mode="determinate")
                self.busy_progress["value"] = max(float(self.busy_progress["value"]), 3.0)
                self.configure(cursor="watch")
            else:
                self.busy_progress["value"] = 0
                self.configure(cursor="")

    def set_operation_progress(self, percent: float, message: Optional[str] = None) -> None:
        if hasattr(self, "busy_progress"):
            self.busy_progress["value"] = max(0.0, min(100.0, percent))
        if message:
            self.action_status_var.set(message)

    def set_scan_busy(self, busy: bool, message: str) -> None:
        self.scan_status_var.set(message)
        self.action_status_var.set(message)
        self.set_busy_indicator(busy)
        if hasattr(self, "scan_button"):
            self.scan_button.configure(state=("disabled" if busy else "normal"))

    def set_read_busy(self, busy: bool, message: str) -> None:
        self.read_status_var.set(message)
        self.action_status_var.set(message)
        self.set_busy_indicator(busy)
        if hasattr(self, "read_once_button"):
            self.read_once_button.configure(state=("disabled" if busy else "normal"))

    def set_change_busy(self, busy: bool, message: str) -> None:
        self.change_status_var.set(message)
        self.action_status_var.set(message)
        self.set_busy_indicator(busy)
        if hasattr(self, "change_address_button"):
            self.change_address_button.configure(state=("disabled" if busy else "normal"))

    def set_raw_busy(self, busy: bool, message: str) -> None:
        self.raw_status_var.set(message)
        self.action_status_var.set(message)
        self.set_busy_indicator(busy)
        if hasattr(self, "raw_send_button"):
            self.raw_send_button.configure(state=("disabled" if busy else "normal"))

    def apply_profile_baud(self, profile_label: str, source: str = "profile") -> None:
        profile = self.registry.by_label(profile_label)
        if not profile:
            return
        baud = str(profile.get("baud", 9600))
        self.baud_var.set(baud)
        msg = f"{source.capitalize()}: baud set from profile → {baud} 8N1"
        self.action_status_var.set(msg)
        if source == "read":
            self.read_status_var.set(msg)
        elif source == "scan":
            self.scan_status_var.set(msg)
        elif source == "change":
            self.change_status_var.set(msg)

    def copy_to_clipboard(self, text: str) -> None:
        self.clipboard_clear()
        self.clipboard_append(text)
        self.update_idletasks()
        self.action_status_var.set("Copied to clipboard")

    def copy_text_widget_all(self, widget: tk.Text) -> None:
        self.copy_to_clipboard(widget.get("1.0", "end-1c"))

    def append_text_widget(self, widget: tk.Text, text: str) -> None:
        widget.insert("end", text)
        widget.see("end")

    def set_raw_example(self, hex_text: str) -> None:
        self.raw_hex_var.set(hex_text)
        self.update_raw_crc_preview()

    def update_raw_crc_preview(self) -> None:
        if not hasattr(self, "raw_tx_preview_var"):
            return
        try:
            req = parse_hex_string(self.raw_hex_var.get())
            if not req:
                self.raw_tx_preview_var.set("TX preview: empty")
                return
            if self.raw_auto_crc_var.get():
                tx_full = append_crc(req)
                crc = modbus_crc16(req)
                self.raw_tx_preview_var.set(
                    f"TX full with CRC: {bytes_to_hex(tx_full)}    |    CRC: {crc & 0xFF:02X} {(crc >> 8) & 0xFF:02X}"
                )
            else:
                crc_state = "CRC OK" if validate_crc(req) else "CRC not checked / invalid / missing"
                self.raw_tx_preview_var.set(f"TX full as typed: {bytes_to_hex(req)}    |    {crc_state}")
        except Exception as exc:
            self.raw_tx_preview_var.set(f"TX preview error: {exc}")

    def copy_raw_tx_preview(self) -> None:
        try:
            req = parse_hex_string(self.raw_hex_var.get())
            if not req:
                raise RuntimeError("HEX request is empty")
            tx = append_crc(req) if self.raw_auto_crc_var.get() else req
            self.copy_to_clipboard(bytes_to_hex(tx))
        except Exception as exc:
            self.show_error("Copy TX full", str(exc))

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def refresh_ports(self) -> None:
        ports = self.transport.list_ports()
        self.port_combo["values"] = ports
        if ports and not self.port_var.get().strip():
            self.port_var.set(ports[0])
        if not ports:
            self.status_var.set("No serial ports found")
            self.action_status_var.set("No COM ports detected")
        else:
            if self.transport.is_open():
                self.status_var.set(f"Connected: {self.transport.port} @ {self.transport.baud} 8N1")
            else:
                self.status_var.set(f"Found {len(ports)} serial port(s)")

    def connect_clicked(self) -> None:
        try:
            baud = self.selected_baud()
            self.transport.ensure_open(self.selected_port(), baud, self.get_timeout_ms())
            self.status_var.set(f"Connected: {self.transport.port} @ {self.transport.baud} 8N1")
            self.action_status_var.set(f"Connected: {self.transport.port} @ {self.transport.baud} 8N1")
            self.log(f"Connected: {self.transport.port} @ {self.transport.baud} 8N1")
        except Exception as exc:
            self.show_error("Connection error", str(exc))

    def disconnect_clicked(self) -> None:
        self.transport.close()
        self.status_var.set("Disconnected")
        self.action_status_var.set("Disconnected")
        self.log("Disconnected")

    # ------------------------------------------------------------------
    # Scan
    # ------------------------------------------------------------------

    def clear_scan_results(self) -> None:
        for item in self.scan_tree.get_children():
            self.scan_tree.delete(item)
        self.last_records.clear()

    def start_scan(self) -> None:
        try:
            start_addr = parse_address(self.scan_start_addr_var.get())
            end_addr = parse_address(self.scan_end_addr_var.get())
            if start_addr < 1 or end_addr > 247 or start_addr > end_addr:
                raise ValueError("Scan address range must be within 1..247 and start <= end")
            port = self.selected_port()
            if not port:
                raise ValueError("Select COM port first")

            if self.scan_all_profiles_var.get():
                profiles = self.registry.all()
                profile_mode = "all profiles"
            else:
                profile = self.registry.by_label(self.scan_profile_var.get())
                if not profile:
                    raise RuntimeError("Selected profile not found")
                profiles = [profile]
                profile_mode = profile.get("label", "selected profile")

            # Snapshot every Tk variable in the UI thread. Worker threads must not
            # call StringVar.get()/BooleanVar.get(); that is how small utilities
            # become frozen bricks wearing a progress bar.
            scan_config = {
                "start_addr": start_addr,
                "end_addr": end_addr,
                "profiles": profiles,
                "profile_mode": profile_mode,
                "port": port,
                "timeout": self.get_timeout_ms(),
                "retries": self.get_retries(),
                "post_delay": self.get_post_delay_ms(),
                "gap": self.get_scan_gap_ms(),
                "verbose": bool(self.scan_verbose_var.get()),
            }
        except Exception as exc:
            self.show_error("Scan setup error", str(exc))
            return

        self.scan_stop_event.clear()
        total = (end_addr - start_addr + 1) * max(1, len(scan_config["profiles"]))
        self.set_scan_busy(True, f"SCAN started: {profile_mode}, {total} request(s). Press Stop to cancel after current request.")
        self.set_operation_progress(0, "SCAN started")
        if hasattr(self, "scan_live_text"):
            self.scan_live_text.insert("end", f"\n{'-' * 90}\n[{now_log_time()}] SCAN STARTED: {profile_mode}, range {format_address(start_addr)}..{format_address(end_addr)}, total={total}\n")
            self.scan_live_text.see("end")
        self.log(f"Scan started: {profile_mode}, range {start_addr}..{end_addr}, total={total}")
        threading.Thread(target=self.scan_worker, args=(scan_config,), daemon=True).start()

    def stop_scan(self) -> None:
        self.scan_stop_event.set()
        self.scan_status_var.set("SCAN: stop requested. Current serial request will finish, then scan will stop.")
        self.action_status_var.set("SCAN: stop requested")
        self.log("Scan stop requested")

    def append_scan_live_log(self, text: str) -> None:
        if hasattr(self, "scan_live_text"):
            self.scan_live_text.insert("end", text)
            self.scan_live_text.see("end")

    def scan_worker(self, scan_config: Dict[str, Any]) -> None:
        try:
            start_addr = int(scan_config["start_addr"])
            end_addr = int(scan_config["end_addr"])
            profiles = list(scan_config["profiles"])
            timeout = int(scan_config["timeout"])
            retries = int(scan_config["retries"])
            post_delay = int(scan_config["post_delay"])
            gap = int(scan_config["gap"])
            port = str(scan_config["port"])
            verbose = bool(scan_config.get("verbose", True))
            total = (end_addr - start_addr + 1) * max(1, len(profiles))
            scanned = 0
            seen = set()

            for profile in profiles:
                if self.scan_stop_event.is_set():
                    break
                baud = int(profile.get("baud", 9600))
                probe = profile.get("probe", {})
                req_template = probe.get("request_no_crc")
                resp_len = int(probe.get("response_length", 0))
                prefix_template = probe.get("response_prefix")
                if not req_template or not prefix_template:
                    continue

                self.call_ui(self.log, f"Scanning profile={profile.get('label')} baud={baud} range={start_addr}..{end_addr}")
                for addr in range(start_addr, end_addr + 1):
                    if self.scan_stop_event.is_set():
                        break
                    scanned += 1
                    percent = (scanned / max(1, total)) * 100.0
                    msg = f"SCAN {scanned}/{total}: {profile.get('label')} @ {baud}, checking {format_address(addr)}"
                    self.call_ui(self.scan_status_var.set, msg)
                    self.call_ui(self.set_operation_progress, percent, msg)
                    try:
                        req_no_crc = replace_template_tokens(req_template, addr)
                        req = append_crc(req_no_crc)
                        prefix = expected_prefix_from_template(prefix_template, addr)
                        if verbose:
                            self.call_ui(self.append_scan_live_log, f"[{now_log_time()}] TX {format_address(addr)} {profile.get('label')}: {bytes_to_hex(req)}\n")
                        result = self.transport.transaction(
                            port_display=port,
                            baud=baud,
                            request=req,
                            expected_prefix=prefix,
                            expected_length=resp_len,
                            timeout_ms=timeout,
                            retries=retries,
                            post_delay_ms=post_delay,
                        )
                        rx_hex = bytes_to_hex(result.get("rx", b""))
                        if verbose or result.get("ok"):
                            self.call_ui(
                                self.append_scan_live_log,
                                f"[{now_log_time()}] RX {format_address(addr)} status={result.get('status')} elapsed={result.get('elapsed_ms')} ms: {rx_hex if rx_hex else '<empty>'}\n",
                            )
                        if result.get("ok") and result.get("frame"):
                            frame = result["frame"]
                            quick = self.quick_parse(profile, frame)
                            dedupe_key = (baud, addr, bytes_to_hex(frame[:3]))
                            if dedupe_key not in seen:
                                seen.add(dedupe_key)
                                record = {
                                    "timestamp": now_local_iso(),
                                    "operation": "scan",
                                    "port": SerialTransport.normalize_port(port),
                                    "baud": baud,
                                    "profile_id": profile.get("id"),
                                    "profile_label": profile.get("label"),
                                    "address": addr,
                                    "quick": quick,
                                    "elapsed_ms": result.get("elapsed_ms"),
                                    "crc_status": result.get("status"),
                                    "tx_full": bytes_to_hex(req),
                                    "rx_raw": rx_hex,
                                    "frame": bytes_to_hex(frame),
                                }
                                self.last_records.append(record)
                                self.call_ui(self.add_scan_result, record)
                    except Exception as exc:
                        self.call_ui(self.append_scan_live_log, f"[{now_log_time()}] ERROR {format_address(addr)} {profile.get('label')}: {exc}\n")
                        self.call_ui(self.log, f"Scan error profile={profile.get('label')} addr={addr}: {exc}")
                    if gap:
                        time.sleep(gap / 1000.0)
            final_msg = "SCAN stopped by user" if self.scan_stop_event.is_set() else "SCAN finished"
            self.call_ui(self.append_scan_live_log, f"[{now_log_time()}] {final_msg}\n")
            self.call_ui(self.log, final_msg)
            self.call_ui(self.set_operation_progress, 100 if not self.scan_stop_event.is_set() else 0, final_msg)
            self.call_ui(self.set_scan_busy, False, final_msg)
        except Exception as exc:
            self.call_ui(self.set_scan_busy, False, "SCAN failed")
            self.call_ui(self.show_error, "Scan error", str(exc))

    def quick_parse(self, profile: Dict[str, Any], frame: bytes) -> str:
        probe_spec = profile.get("probe", {})
        if probe_spec.get("fields"):
            values = ProfileParser.parse_spec(probe_spec, frame)
        else:
            values = ProfileParser.parse(profile, frame)
        parts = []
        for v in values[:3]:
            parts.append(f"{v.label}: {v.value} {v.unit}".strip())
        return ", ".join(parts)

    def add_scan_result(self, record: Dict[str, Any]) -> None:
        self.scan_tree.insert("", "end", values=(
            record.get("timestamp", ""),
            record.get("baud", ""),
            format_address(int(record.get("address", 0))),
            record.get("profile_label", ""),
            record.get("quick", ""),
            record.get("elapsed_ms", ""),
            record.get("crc_status", ""),
            record.get("rx_raw", ""),
        ))
        self.log_block("SCAN HIT", [
            f"PORT: {record.get('port')}",
            f"BAUD: {record.get('baud')}",
            f"PROFILE: {record.get('profile_label')}",
            f"ADDRESS: {format_address(int(record.get('address', 0)))}",
            f"TX: {record.get('tx_full')}",
            f"RX: {record.get('rx_raw')}",
            f"FRAME: {record.get('frame')}",
            f"RESULT: {record.get('quick')}",
            f"STATUS: {record.get('crc_status')}",
        ])

    # ------------------------------------------------------------------
    # Read
    # ------------------------------------------------------------------

    def read_once_clicked(self) -> None:
        try:
            profile = self.registry.by_label(self.read_profile_var.get())
            if not profile:
                raise RuntimeError("Selected profile not found")
            addr = parse_address(self.read_addr_var.get())
            read_config = {
                "profile": profile,
                "addr": addr,
                "port": self.selected_port(),
                "timeout": self.get_timeout_ms(),
                "retries": self.get_retries(),
                "post_delay": self.get_post_delay_ms(),
            }
            if not read_config["port"]:
                raise RuntimeError("Select COM port first")
        except Exception as exc:
            self.show_error("Read setup error", str(exc))
            return

        self.apply_profile_baud(profile.get("label", ""), "read")
        self.set_read_busy(True, f"READ: sending {profile.get('label')} request to {format_address(addr)}...")
        self.set_operation_progress(10, "READ request prepared")
        self.read_raw_text.delete("1.0", "end")
        self.read_raw_text.insert(
            "end",
            f"[{now_log_time()}] READ started\nProfile: {profile.get('label')}\nAddress: {format_address(addr)}\nPort: {SerialTransport.normalize_port(read_config['port'])}\n",
        )
        threading.Thread(target=self.read_once_worker, args=(read_config,), daemon=True).start()

    def read_once_worker(self, read_config: Dict[str, Any]) -> None:
        try:
            profile = read_config["profile"]
            addr = int(read_config["addr"])
            record = self.read_profile_address(
                profile,
                addr,
                port=str(read_config["port"]),
                timeout=int(read_config["timeout"]),
                retries=int(read_config["retries"]),
                post_delay=int(read_config["post_delay"]),
            )
            self.call_ui(self.show_read_result, profile, record)
        except Exception as exc:
            self.call_ui(self.set_read_busy, False, "READ failed")
            self.call_ui(self.show_error, "Read error", str(exc))

    def read_profile_address(self, profile: Dict[str, Any], addr: int, port: str, timeout: int, retries: int, post_delay: int) -> Dict[str, Any]:
        read_spec = profile.get("read", {})
        read_blocks = read_spec.get("blocks")
        if not read_blocks:
            read_blocks = [read_spec]
        if not isinstance(read_blocks, list) or not read_blocks:
            raise RuntimeError("Selected profile has no readable blocks")

        baud = int(profile.get("baud", 9600))
        block_delay_ms = max(0, int(read_spec.get("block_delay_ms", 0)))
        transactions: List[Dict[str, Any]] = []
        values: List[ParsedValue] = []
        total_elapsed_ms = 0
        successful_blocks = 0

        for index, block in enumerate(read_blocks, start=1):
            block_label = str(block.get("label", f"Read block {index}"))
            req_no_crc = replace_template_tokens(block["request_no_crc"], addr)
            req = append_crc(req_no_crc)
            prefix = expected_prefix_from_template(block["response_prefix"], addr)
            resp_len = int(block["response_length"])

            progress = 15.0 + ((index - 1) / max(1, len(read_blocks))) * 75.0
            self.call_ui(
                self.set_operation_progress,
                progress,
                f"READ {index}/{len(read_blocks)}: {block_label}",
            )
            self.call_ui(
                self.append_text_widget,
                self.read_raw_text,
                (
                    f"\n[{index}/{len(read_blocks)}] {block_label}\n"
                    f"TX no CRC: {bytes_to_hex(req_no_crc)}\n"
                    f"TX full:   {bytes_to_hex(req)}\n"
                    f"Waiting for RX, timeout={timeout} ms, retries={retries}...\n"
                ),
            )
            result = self.transport.transaction(
                port_display=port,
                baud=baud,
                request=req,
                expected_prefix=prefix,
                expected_length=resp_len,
                timeout_ms=timeout,
                retries=retries,
                post_delay_ms=post_delay,
            )
            frame = result.get("frame")
            elapsed_ms = int(result.get("elapsed_ms") or 0)
            total_elapsed_ms += elapsed_ms
            if frame and result.get("ok"):
                successful_blocks += 1
                values.extend(ProfileParser.parse_spec(block, frame))
            else:
                values.extend(
                    ProfileParser.unavailable_values(
                        block, f"NO VALID RESPONSE: {result.get('status')}"
                    )
                )

            transaction = {
                "label": block_label,
                "tx_no_crc": bytes_to_hex(req_no_crc),
                "tx_full": bytes_to_hex(req),
                "rx_raw": bytes_to_hex(result.get("rx", b"")),
                "frame": bytes_to_hex(frame) if frame else "",
                "ok": bool(result.get("ok")),
                "status": result.get("status"),
                "elapsed_ms": elapsed_ms,
            }
            transactions.append(transaction)
            self.call_ui(
                self.append_text_widget,
                self.read_raw_text,
                (
                    f"RX raw:    {transaction['rx_raw'] or '<empty>'}\n"
                    f"Frame:     {transaction['frame'] or '<none>'}\n"
                    f"Status:    {transaction['status']} | {elapsed_ms} ms\n"
                ),
            )
            if block_delay_ms > 0 and index < len(read_blocks):
                time.sleep(block_delay_ms / 1000.0)

        self.call_ui(self.set_operation_progress, 90, "READ: response processed")

        total_blocks = len(read_blocks)
        if successful_blocks == total_blocks:
            status = f"ALL BLOCKS OK ({successful_blocks}/{total_blocks})"
        elif successful_blocks > 0:
            status = f"PARTIAL ({successful_blocks}/{total_blocks} blocks OK)"
        else:
            status = f"NO VALID RESPONSES (0/{total_blocks})"

        def join_transaction_field(key: str) -> str:
            return " | ".join(
                f"{item['label']}: {item.get(key, '')}" for item in transactions
            )

        record = {
            "timestamp": now_local_iso(),
            "operation": "read",
            "port": SerialTransport.normalize_port(port),
            "baud": baud,
            "profile_id": profile.get("id"),
            "profile_label": profile.get("label"),
            "address": addr,
            "tx_no_crc": join_transaction_field("tx_no_crc"),
            "tx_full": join_transaction_field("tx_full"),
            "rx_raw": join_transaction_field("rx_raw"),
            "frame": join_transaction_field("frame"),
            "crc_ok": successful_blocks == total_blocks,
            "response_ok": successful_blocks > 0,
            "status": status,
            "elapsed_ms": total_elapsed_ms,
            "successful_blocks": successful_blocks,
            "total_blocks": total_blocks,
            "transactions": transactions,
            "values": [v.__dict__ for v in values],
        }
        self.last_records.append(record)
        return record

    def show_read_result(self, profile: Dict[str, Any], record: Dict[str, Any]) -> None:
        for item in self.read_tree.get_children():
            self.read_tree.delete(item)

        for value in record.get("values", []):
            self.read_tree.insert("", "end", values=(
                value.get("label", ""),
                value.get("value", ""),
                value.get("unit", ""),
                value.get("raw", ""),
                value.get("status", ""),
            ))

        self.read_raw_text.delete("1.0", "end")
        lines = [
            f"Timestamp: {record.get('timestamp')}",
            f"Profile:   {record.get('profile_label')}",
            f"Address:   {format_address(int(record.get('address', 0)))}",
            f"Port:      {record.get('port')}",
            f"Baud:      {record.get('baud')}",
            f"Status:    {record.get('status')}",
            f"Elapsed:   {record.get('elapsed_ms')} ms",
        ]
        transactions = record.get("transactions", [])
        if transactions:
            for index, transaction in enumerate(transactions, start=1):
                lines.extend([
                    "",
                    f"[{index}/{len(transactions)}] {transaction.get('label')}",
                    f"TX no CRC: {transaction.get('tx_no_crc')}",
                    f"TX full:   {transaction.get('tx_full')}",
                    f"RX raw:    {transaction.get('rx_raw') or '<empty>'}",
                    f"Frame:     {transaction.get('frame') or '<none>'}",
                    f"Status:    {transaction.get('status')}",
                    f"Elapsed:   {transaction.get('elapsed_ms')} ms",
                ])
        else:
            lines.extend([
                f"TX no CRC: {record.get('tx_no_crc')}",
                f"TX full:   {record.get('tx_full')}",
                f"RX raw:    {record.get('rx_raw')}",
                f"Frame:     {record.get('frame')}",
            ])
        self.read_raw_text.insert("end", "\n".join(lines))

        if record.get("crc_ok") and record.get("values"):
            self.set_read_busy(False, f"READ OK: {record.get('profile_label')} at {format_address(int(record.get('address', 0)))} | {record.get('elapsed_ms')} ms")
        elif record.get("response_ok"):
            self.set_read_busy(False, f"READ PARTIAL: {record.get('status')} | {record.get('elapsed_ms')} ms")
        elif record.get("crc_ok"):
            self.set_read_busy(False, f"READ response received, but no parsed fields | {record.get('elapsed_ms')} ms")
        else:
            self.set_read_busy(False, f"READ failed/no valid frame: {record.get('status')} | {record.get('elapsed_ms')} ms")

        self.log_block("READ", lines + ["RESULTS:"] + [
            f"  {v.get('label')} = {v.get('value')} {v.get('unit')} raw={v.get('raw')} status={v.get('status')}"
            for v in record.get("values", [])
        ])

    def continuous_read_toggled(self) -> None:
        if self.cont_read_var.get():
            try:
                profile = self.registry.by_label(self.read_profile_var.get())
                if not profile:
                    raise RuntimeError("Selected profile not found")
                config = {
                    "profile": profile,
                    "addr": parse_address(self.read_addr_var.get()),
                    "port": self.selected_port(),
                    "timeout": self.get_timeout_ms(),
                    "retries": self.get_retries(),
                    "post_delay": self.get_post_delay_ms(),
                    "interval": max(100, int(self.read_interval_var.get().strip())),
                }
                if not config["port"]:
                    raise RuntimeError("Select COM port first")
            except Exception as exc:
                self.cont_read_var.set(False)
                self.show_error("Continuous read setup error", str(exc))
                return
            self.continuous_stop_event.clear()
            threading.Thread(target=self.continuous_read_worker, args=(config,), daemon=True).start()
        else:
            self.continuous_stop_event.set()

    def stop_continuous_read(self) -> None:
        self.continuous_stop_event.set()
        self.cont_read_var.set(False)
        self.log("Continuous read stopped")

    def continuous_read_worker(self, config: Dict[str, Any]) -> None:
        self.call_ui(self.log, "Continuous read started")
        while not self.continuous_stop_event.is_set():
            try:
                record = self.read_profile_address(
                    config["profile"],
                    int(config["addr"]),
                    port=str(config["port"]),
                    timeout=int(config["timeout"]),
                    retries=int(config["retries"]),
                    post_delay=int(config["post_delay"]),
                )
                self.call_ui(self.show_read_result, config["profile"], record)
            except Exception as exc:
                self.call_ui(self.log, f"Continuous read error: {exc}")
            wait_until = time.time() + int(config.get("interval", 1000)) / 1000.0
            while time.time() < wait_until:
                if self.continuous_stop_event.is_set():
                    break
                time.sleep(0.05)
        self.call_ui(self.log, "Continuous read finished")

    # ------------------------------------------------------------------
    # Change address
    # ------------------------------------------------------------------

    def update_change_confirmations(self) -> None:
        if not hasattr(self, "confirm_wire_check"):
            return
        profile = self.registry.by_label(self.change_profile_var.get())
        change = profile.get("change_address", {}) if profile else {}
        supported = bool(change.get("supported", False))
        requires_wire = bool(change.get("requires_wire_note") or (profile and profile.get("id") == "rika_leaf_sensor"))

        # Reset confirmations when profile changes. Otherwise a confirmation for
        # one sensor silently carries into another sensor, which is how accidents
        # receive a UI budget.
        self.confirm_single_device_var.set(False)
        self.confirm_power_var.set(False)
        self.confirm_risk_var.set(False)
        self.confirm_wire_var.set(False)

        normal_state = "normal" if supported else "disabled"
        for widget in (self.confirm_single_device_check, self.confirm_power_check, self.confirm_risk_check):
            widget.configure(state=normal_state)

        if requires_wire and supported:
            note = change.get("requires_wire_note", "This profile has an extra wiring condition for address change.")
            self.confirm_wire_check.configure(text=f"✓ {note}", state="normal")
            self.confirm_wire_check.pack(anchor="w", pady=2)
        else:
            self.confirm_wire_check.pack_forget()

        if hasattr(self, "change_address_button"):
            self.change_address_button.configure(state=("normal" if supported else "disabled"))

        if not profile:
            self.confirm_profile_note_var.set("No profile selected.")
        elif not supported:
            self.confirm_profile_note_var.set(f"Address change is not implemented for this profile: {profile.get('label')}")
        elif requires_wire:
            self.confirm_profile_note_var.set(f"Extra condition for {profile.get('label')}: {change.get('requires_wire_note', 'check profile wiring note')}")
        else:
            self.confirm_profile_note_var.set(f"Required confirmations for {profile.get('label')}: single device, stable wiring, risk acknowledgment.")

    def update_change_preview(self) -> None:
        if not hasattr(self, "change_preview_text"):
            return
        self.change_preview_text.delete("1.0", "end")
        try:
            profile = self.registry.by_label(self.change_profile_var.get())
            if not profile:
                self.change_preview_text.insert("end", "Selected profile not found")
                return
            current_addr = parse_address(self.change_current_addr_var.get())
            new_addr = parse_address(self.change_new_addr_var.get())
            change = profile.get("change_address", {})
            lines = [
                f"Profile: {profile.get('label')}",
                f"Baud: {profile.get('baud')} 8N1",
                f"Current address: {format_address(current_addr)}",
                f"New address: {format_address(new_addr)}",
                "",
            ]
            if not change.get("supported", False):
                lines.append("Address change is NOT supported for this profile.")
                lines.append(change.get("warning", ""))
            elif change.get("mode") == "ascii_sequence":
                lines += [
                    "Address change mode: proprietary ASCII sequence, not Modbus.",
                    "TX sequence:",
                    "  >*\\r\\n",
                    f"  >ID {new_addr:02d}\\r\\n",
                    "  >!\\r\\n",
                    "Optional reboot is not sent automatically in this MVP.",
                    "",
                    change.get("warning", ""),
                ]
            elif change.get("mode") == "epever_custom_45":
                find_no_crc = build_epever_service_find_id_request_no_crc()
                find_full = append_crc(find_no_crc)
                req_no_crc = build_epever_service_set_id_request_no_crc(new_addr)
                req = append_crc(req_no_crc)
                lines += [
                    "Address change mode: EPEVER proprietary service command 0x45",
                    "This is NOT normal Modbus FC06/FC10.",
                    f"Find/read ID TX no CRC: {bytes_to_hex(find_no_crc)}",
                    f"Find/read ID TX full:   {bytes_to_hex(find_full)}",
                    f"Set ID TX no CRC:       {bytes_to_hex(req_no_crc)}",
                    f"Set ID TX full:         {bytes_to_hex(req)}",
                    "Verification: app probes old and new address with FC04 battery-voltage read.",
                    "",
                    change.get("warning", ""),
                ]
            else:
                req_no_crc = replace_template_tokens(change["request_no_crc"], current_addr, new_addr)
                req = append_crc(req_no_crc)
                lines += [
                    "Address change mode: Modbus template",
                    f"TX no CRC: {bytes_to_hex(req_no_crc)}",
                    f"TX full:   {bytes_to_hex(req)}",
                    f"Expected response prefix: {change.get('response_prefix')}",
                    f"Expected response length: {change.get('response_length')} bytes",
                    "",
                    change.get("warning", ""),
                ]
                if change.get("requires_wire_note"):
                    lines.append(change.get("requires_wire_note"))
            self.change_preview_text.insert("end", "\n".join(lines))
        except Exception as exc:
            self.change_preview_text.insert("end", f"Preview error: {exc}")

    def change_address_clicked(self) -> None:
        try:
            profile = self.registry.by_label(self.change_profile_var.get())
            if not profile:
                raise RuntimeError("Selected profile not found")
            current_addr = parse_address(self.change_current_addr_var.get())
            new_addr = parse_address(self.change_new_addr_var.get())
            if not 1 <= new_addr <= 247:
                raise RuntimeError("New address must be 1..247")
            if current_addr == new_addr:
                raise RuntimeError("Current and new address are the same")
            change = profile.get("change_address", {})
            if not change.get("supported", False):
                raise RuntimeError("Address change is not supported for this profile")
            if not self.confirm_single_device_var.get() or not self.confirm_power_var.get() or not self.confirm_risk_var.get():
                raise RuntimeError("Confirm single device, stable wiring and address-change risk before continuing")
            requires_wire = bool(change.get("requires_wire_note") or profile.get("id") == "rika_leaf_sensor")
            if requires_wire and not self.confirm_wire_var.get():
                raise RuntimeError("This profile requires extra wiring confirmation before address change")
            change_config = {
                "profile": profile,
                "current_addr": current_addr,
                "new_addr": new_addr,
                "port": self.selected_port(),
                "timeout": self.get_timeout_ms(),
                "retries": self.get_retries(),
                "post_delay": self.get_post_delay_ms(),
            }
            if not change_config["port"]:
                raise RuntimeError("Select COM port first")
        except Exception as exc:
            self.show_error("Address change setup error", str(exc))
            return

        self.apply_profile_baud(profile.get("label", ""), "change")
        self.set_change_busy(True, f"CHANGE ADDRESS: sending command for {profile.get('label')} {format_address(current_addr)} → {format_address(new_addr)}")
        self.set_operation_progress(10, "CHANGE ADDRESS: command prepared")
        self.change_preview_text.insert("end", "\n\n--- Sending address-change command now. Do not disconnect power or RS485 lines. ---\n")
        self.change_preview_text.see("end")
        threading.Thread(target=self.change_address_worker, args=(change_config,), daemon=True).start()

    def change_address_worker(self, change_config: Dict[str, Any]) -> None:
        try:
            profile = change_config["profile"]
            current_addr = int(change_config["current_addr"])
            new_addr = int(change_config["new_addr"])
            change = profile.get("change_address", {})
            if change.get("mode") == "ascii_sequence":
                record = self.change_address_ascii(profile, current_addr, new_addr, change_config)
            elif change.get("mode") == "epever_custom_45":
                record = self.change_address_epever_custom45(profile, current_addr, new_addr, change_config)
            else:
                record = self.change_address_modbus(profile, current_addr, new_addr, change_config)
            self.call_ui(self.show_change_result, record)
        except Exception as exc:
            self.call_ui(self.set_change_busy, False, "CHANGE ADDRESS failed")
            self.call_ui(self.show_error, "Address change error", str(exc))

    def change_address_modbus(self, profile: Dict[str, Any], current_addr: int, new_addr: int, change_config: Dict[str, Any]) -> Dict[str, Any]:
        change = profile["change_address"]
        baud = int(profile.get("baud", 9600))
        req_no_crc = replace_template_tokens(change["request_no_crc"], current_addr, new_addr)
        req = append_crc(req_no_crc)
        prefix = expected_prefix_from_template(change["response_prefix"], current_addr, new_addr)
        resp_len = int(change["response_length"])
        port = str(change_config["port"])
        timeout = int(change_config["timeout"])
        retries = int(change_config["retries"])
        post_delay = int(change_config["post_delay"])

        self.call_ui(self.set_operation_progress, 25, "CHANGE ADDRESS: TX sent, waiting for write echo")
        self.call_ui(
            self.append_text_widget,
            self.change_preview_text,
            f"\n[{now_log_time()}] TX full sent: {bytes_to_hex(req)}\nWaiting for write echo, timeout={timeout} ms, retries={retries}...\n",
        )
        result = self.transport.transaction(
            port_display=port,
            baud=baud,
            request=req,
            expected_prefix=prefix,
            expected_length=resp_len,
            timeout_ms=timeout,
            retries=retries,
            post_delay_ms=post_delay,
        )
        self.call_ui(self.set_operation_progress, 55, "CHANGE ADDRESS: write done, verifying new address")
        time.sleep(0.5)

        verify_new = self.safe_probe(profile, new_addr, port=port, timeout=timeout, post_delay=post_delay)
        self.call_ui(self.set_operation_progress, 75, "CHANGE ADDRESS: verifying old address")
        verify_old = self.safe_probe(profile, current_addr, port=port, timeout=timeout, post_delay=post_delay)
        self.call_ui(self.set_operation_progress, 95, "CHANGE ADDRESS: verification complete")

        if result.get("ok") and verify_new.get("ok") and not verify_old.get("ok"):
            final_status = "SUCCESS: new address responds, old address does not respond"
        elif result.get("ok") and verify_new.get("ok") and verify_old.get("ok"):
            final_status = "DANGEROUS/PARTIAL: both old and new addresses respond"
        elif result.get("ok") and not verify_new.get("ok"):
            final_status = "PARTIAL: write echo OK, but new address did not respond"
        else:
            final_status = "FAILED: no valid write echo"

        record = {
            "timestamp": now_local_iso(),
            "operation": "change_address",
            "profile_id": profile.get("id"),
            "profile_label": profile.get("label"),
            "port": SerialTransport.normalize_port(port),
            "baud": baud,
            "current_address": current_addr,
            "new_address": new_addr,
            "tx_no_crc": bytes_to_hex(req_no_crc),
            "tx_full": bytes_to_hex(req),
            "rx_raw": bytes_to_hex(result.get("rx", b"")),
            "frame": bytes_to_hex(result.get("frame", b"") or b""),
            "write_status": result.get("status"),
            "verify_new_status": verify_new.get("status"),
            "verify_old_status": verify_old.get("status"),
            "final_status": final_status,
        }
        self.last_records.append(record)
        return record

    def change_address_ascii(self, profile: Dict[str, Any], current_addr: int, new_addr: int, change_config: Dict[str, Any]) -> Dict[str, Any]:
        baud = int(profile.get("baud", 9600))
        timeout = int(change_config["timeout"])
        port = str(change_config["port"])
        post_delay = int(change_config["post_delay"])
        steps = [
            ("enter_config", ">*\r\n"),
            ("set_address", f">ID {new_addr:02d}\r\n"),
            ("exit_config", ">!\r\n"),
        ]
        step_results = []
        for idx, (name, text) in enumerate(steps, start=1):
            self.call_ui(self.set_operation_progress, 20 + idx * 15, f"ASCII address change: {name}")
            res = self.transport.write_ascii_and_read(port, baud, text, timeout)
            step_results.append({
                "step": name,
                "tx_ascii": repr(text),
                "tx_hex": bytes_to_hex(res["tx"]),
                "rx_hex": bytes_to_hex(res["rx"]),
                "rx_ascii": decode_ascii_preview(res["rx"]),
                "elapsed_ms": res["elapsed_ms"],
            })
            time.sleep(0.3)
        time.sleep(0.5)
        verify_new = self.safe_probe(profile, new_addr, port=port, timeout=timeout, post_delay=post_delay)
        verify_old = self.safe_probe(profile, current_addr, port=port, timeout=timeout, post_delay=post_delay)
        if verify_new.get("ok") and not verify_old.get("ok"):
            final_status = "SUCCESS: new address responds, old address does not respond"
        elif verify_new.get("ok") and verify_old.get("ok"):
            final_status = "DANGEROUS/PARTIAL: both old and new addresses respond"
        else:
            final_status = "PARTIAL/FAILED: new address did not respond"
        record = {
            "timestamp": now_local_iso(),
            "operation": "change_address_ascii",
            "profile_id": profile.get("id"),
            "profile_label": profile.get("label"),
            "port": SerialTransport.normalize_port(port),
            "baud": baud,
            "current_address": current_addr,
            "new_address": new_addr,
            "steps": step_results,
            "verify_new_status": verify_new.get("status"),
            "verify_old_status": verify_old.get("status"),
            "final_status": final_status,
        }
        self.last_records.append(record)
        return record

    def change_address_epever_custom45(self, profile: Dict[str, Any], current_addr: int, new_addr: int, change_config: Dict[str, Any]) -> Dict[str, Any]:
        baud = int(profile.get("baud", 115200))
        timeout = int(change_config["timeout"])
        retries = int(change_config["retries"])
        post_delay = int(change_config["post_delay"])
        port = str(change_config["port"])

        old_probe = self.safe_probe(profile, current_addr, port=port, timeout=timeout, post_delay=post_delay)

        find_no_crc = build_epever_service_find_id_request_no_crc()
        find_req = append_crc(find_no_crc)
        self.call_ui(self.set_operation_progress, 25, "EPEVER ID: sending custom find/read ID frame")
        self.call_ui(self.append_text_widget, self.change_preview_text, f"\n[{now_log_time()}] EPEVER FIND/READ ID\nTX full: {bytes_to_hex(find_req)}\n")
        find_res = self.transport.transaction(
            port_display=port,
            baud=baud,
            request=find_req,
            expected_prefix=None,
            expected_length=None,
            timeout_ms=timeout,
            retries=max(1, retries),
            post_delay_ms=post_delay,
        )
        self.call_ui(self.append_text_widget, self.change_preview_text, f"RX raw:  {bytes_to_hex(find_res.get('rx', b''))}\nStatus:  {find_res.get('status')} | elapsed={find_res.get('elapsed_ms')} ms\n")

        req_no_crc = build_epever_service_set_id_request_no_crc(new_addr)
        req = append_crc(req_no_crc)
        self.call_ui(self.set_operation_progress, 45, "EPEVER ID: sending custom set ID frame")
        self.call_ui(self.append_text_widget, self.change_preview_text, f"\n[{now_log_time()}] EPEVER SET ID {format_address(new_addr)}\nTX no CRC: {bytes_to_hex(req_no_crc)}\nTX full:   {bytes_to_hex(req)}\nWaiting for response, then verifying by normal FC04 reads...\n")
        write_res = self.transport.transaction(
            port_display=port,
            baud=baud,
            request=req,
            expected_prefix=None,
            expected_length=None,
            timeout_ms=timeout,
            retries=max(1, retries),
            post_delay_ms=post_delay,
        )
        self.call_ui(self.append_text_widget, self.change_preview_text, f"RX raw:    {bytes_to_hex(write_res.get('rx', b''))}\nStatus:    {write_res.get('status')} | elapsed={write_res.get('elapsed_ms')} ms\n")

        self.call_ui(self.set_operation_progress, 65, "EPEVER ID: waiting before verification")
        time.sleep(0.8)
        self.call_ui(self.set_operation_progress, 78, "EPEVER ID: verifying new address")
        verify_new = self.safe_probe(profile, new_addr, port=port, timeout=timeout, post_delay=post_delay)
        self.call_ui(self.set_operation_progress, 92, "EPEVER ID: verifying old address")
        verify_old = self.safe_probe(profile, current_addr, port=port, timeout=timeout, post_delay=post_delay)

        if verify_new.get("ok") and not verify_old.get("ok"):
            final_status = "SUCCESS: new EPEVER address responds, old address does not respond"
        elif verify_new.get("ok") and verify_old.get("ok"):
            final_status = "DANGEROUS/PARTIAL: both old and new EPEVER addresses respond"
        elif write_res.get("ok") and not verify_new.get("ok"):
            final_status = "PARTIAL: custom write got response, but new address did not respond"
        else:
            final_status = "FAILED: new address did not respond"

        record = {
            "timestamp": now_local_iso(),
            "operation": "change_address_epever_custom45",
            "profile_id": profile.get("id"),
            "profile_label": profile.get("label"),
            "port": SerialTransport.normalize_port(port),
            "baud": baud,
            "current_address": current_addr,
            "new_address": new_addr,
            "find_tx_full": bytes_to_hex(find_req),
            "find_rx_raw": bytes_to_hex(find_res.get("rx", b"")),
            "old_probe_before_status": old_probe.get("status"),
            "tx_no_crc": bytes_to_hex(req_no_crc),
            "tx_full": bytes_to_hex(req),
            "rx_raw": bytes_to_hex(write_res.get("rx", b"")),
            "write_status": write_res.get("status"),
            "verify_new_status": verify_new.get("status"),
            "verify_old_status": verify_old.get("status"),
            "verify_new_rx": bytes_to_hex(verify_new.get("rx", b"")),
            "verify_old_rx": bytes_to_hex(verify_old.get("rx", b"")),
            "final_status": final_status,
        }
        self.last_records.append(record)
        return record

    def safe_probe(self, profile: Dict[str, Any], addr: int, port: str, timeout: int, post_delay: int) -> Dict[str, Any]:
        probe = profile.get("probe", {})
        req_no_crc = replace_template_tokens(probe["request_no_crc"], addr)
        req = append_crc(req_no_crc)
        prefix = expected_prefix_from_template(probe["response_prefix"], addr)
        return self.transport.transaction(
            port_display=port,
            baud=int(profile.get("baud", 9600)),
            request=req,
            expected_prefix=prefix,
            expected_length=int(probe["response_length"]),
            timeout_ms=timeout,
            retries=1,
            post_delay_ms=post_delay,
        )

    def show_change_result(self, record: Dict[str, Any]) -> None:
        lines = [
            f"Timestamp: {record.get('timestamp')}",
            f"Profile: {record.get('profile_label')}",
            f"Port: {record.get('port')}",
            f"Baud: {record.get('baud')}",
            f"Current address: {format_address(int(record.get('current_address', 0)))}",
            f"New address: {format_address(int(record.get('new_address', 0)))}",
            f"Final status: {record.get('final_status')}",
        ]
        if "steps" in record:
            for step in record["steps"]:
                lines += [
                    "",
                    f"Step: {step.get('step')}",
                    f"TX ASCII: {step.get('tx_ascii')}",
                    f"TX HEX:   {step.get('tx_hex')}",
                    f"RX HEX:   {step.get('rx_hex')}",
                    f"RX ASCII: {step.get('rx_ascii')}",
                ]
        else:
            if record.get("find_tx_full"):
                lines += [
                    f"Find/read ID TX: {record.get('find_tx_full')}",
                    f"Find/read ID RX: {record.get('find_rx_raw')}",
                    f"Old probe before: {record.get('old_probe_before_status')}",
                ]
            lines += [
                f"TX no CRC: {record.get('tx_no_crc')}",
                f"TX full:   {record.get('tx_full')}",
                f"RX raw:    {record.get('rx_raw')}",
                f"Frame:     {record.get('frame')}",
                f"Write status: {record.get('write_status')}",
            ]
            if record.get("verify_new_rx") or record.get("verify_old_rx"):
                lines += [
                    f"Verify new RX: {record.get('verify_new_rx')}",
                    f"Verify old RX: {record.get('verify_old_rx')}",
                ]
        lines += [
            f"Verify new: {record.get('verify_new_status')}",
            f"Verify old: {record.get('verify_old_status')}",
        ]
        self.change_preview_text.insert("end", "\n\n" + "\n".join(lines))
        self.change_preview_text.see("end")
        self.set_change_busy(False, f"CHANGE ADDRESS: {record.get('final_status')}")
        self.log_block("CHANGE ADDRESS", lines)

    # ------------------------------------------------------------------
    # Raw Modbus
    # ------------------------------------------------------------------

    def raw_send_clicked(self) -> None:
        try:
            self.update_raw_crc_preview()
            req_no_crc = parse_hex_string(self.raw_hex_var.get())
            if not req_no_crc:
                raise RuntimeError("HEX request is empty")
            auto_crc = bool(self.raw_auto_crc_var.get())
            req = append_crc(req_no_crc) if auto_crc else req_no_crc
            raw_config = {
                "req_no_crc": req_no_crc,
                "req": req,
                "auto_crc": auto_crc,
                "port": self.selected_port(),
                "baud": self.selected_baud(),
                "timeout": self.get_timeout_ms(),
                "retries": self.get_retries(),
                "post_delay": self.get_post_delay_ms(),
            }
            if not raw_config["port"]:
                raise RuntimeError("Select COM port first")
        except Exception as exc:
            self.show_error("Raw send setup error", str(exc))
            return
        self.set_raw_busy(True, "RAW: sending HEX request, waiting for response...")
        self.set_operation_progress(20, "RAW: TX sent")
        self.raw_text.insert("end", f"\n--- RAW request started at {now_log_time()} ---\nTX full: {bytes_to_hex(req)}\n")
        self.raw_text.see("end")
        threading.Thread(target=self.raw_send_worker, args=(raw_config,), daemon=True).start()

    def raw_send_worker(self, raw_config: Dict[str, Any]) -> None:
        try:
            result = self.transport.transaction(
                port_display=str(raw_config["port"]),
                baud=int(raw_config["baud"]),
                request=raw_config["req"],
                expected_prefix=None,
                expected_length=None,
                timeout_ms=int(raw_config["timeout"]),
                retries=int(raw_config["retries"]),
                post_delay_ms=int(raw_config["post_delay"]),
            )
            self.call_ui(self.set_operation_progress, 90, "RAW: response processed")
            record = {
                "timestamp": now_local_iso(),
                "operation": "raw_modbus",
                "port": SerialTransport.normalize_port(str(raw_config["port"])),
                "baud": int(raw_config["baud"]),
                "tx_no_crc": bytes_to_hex(raw_config["req_no_crc"]),
                "tx_full": bytes_to_hex(raw_config["req"]),
                "rx_raw": bytes_to_hex(result.get("rx", b"")),
                "rx_ascii": decode_ascii_preview(result.get("rx", b"")),
                "elapsed_ms": result.get("elapsed_ms"),
                "status": result.get("status"),
                "crc_mode": "auto_appended" if raw_config["auto_crc"] else "as_typed",
            }
            self.last_records.append(record)
            self.call_ui(self.show_raw_result, record)
        except Exception as exc:
            self.call_ui(self.set_raw_busy, False, "RAW failed")
            self.call_ui(self.show_error, "Raw send error", str(exc))

    def show_raw_result(self, record: Dict[str, Any]) -> None:
        lines = [
            "-" * 90,
            f"{record.get('timestamp')} RAW MODBUS",
            f"PORT: {record.get('port')}",
            f"BAUD: {record.get('baud')}",
            f"TX no CRC: {record.get('tx_no_crc')}",
            f"TX full:   {record.get('tx_full')}",
            f"RX raw:    {record.get('rx_raw')}",
            f"RX ascii:  {record.get('rx_ascii')}",
            f"Elapsed:   {record.get('elapsed_ms')} ms",
            f"Status:    {record.get('status')}",
            f"CRC mode:  {record.get('crc_mode')}",
        ]
        self.raw_text.insert("end", "\n".join(lines) + "\n")
        self.raw_text.see("end")
        self.set_raw_busy(False, f"RAW done: {record.get('status')} | {record.get('elapsed_ms')} ms")
        self.log_block("RAW MODBUS", lines[2:])

    # ------------------------------------------------------------------
    # EPEVER G3 config
    # ------------------------------------------------------------------

    def load_epever_preset(self) -> None:
        preset = EPEVER_PRESETS.get(self.epever_preset_var.get(), EPEVER_PRESETS["PKCELL 2x 12V9Ah parallel"])
        self.epever_capacity_var.set(str(preset["capacity_ah"]))
        self.epever_max_charge_current_var.set(f"{float(preset['max_charge_current_a']):.2f}")
        self.epever_rated_voltage_level_var.set(str(preset["rated_voltage_level"]))
        for _reg, _label, key in EPEVER_VOLTAGE_ORDER:
            self.epever_voltage_vars[key].set(f"{float(preset['voltage_block'][key]):.2f}")
        self.epever_status_var.set(f"Loaded preset: {self.epever_preset_var.get()}")

    def get_epever_common_config(self) -> Dict[str, Any]:
        port = self.selected_port()
        if not port:
            raise RuntimeError("Select COM port first")
        address = parse_address(self.epever_addr_var.get())
        if not 1 <= address <= 247:
            raise RuntimeError("EPEVER address must be 1..247")
        return {
            "port": port,
            "address": address,
            "baud": 115200,
            "timeout": self.get_timeout_ms(),
            "retries": self.get_retries(),
            "post_delay": self.get_post_delay_ms(),
        }

    def get_epever_selected_values(self) -> Dict[str, Any]:
        capacity = int(float(self.epever_capacity_var.get().strip()))
        rated_voltage = int(float(self.epever_rated_voltage_level_var.get().strip()))
        max_charge_raw = amps_to_epever_raw(self.epever_max_charge_current_var.get().strip())
        if not 1 <= capacity <= 4000:
            raise RuntimeError("Capacity must be 1..4000 Ah")
        if not 0 <= rated_voltage <= 4:
            raise RuntimeError("Rated voltage level must be 0..4")
        if not 100 <= max_charge_raw <= 10000:
            raise RuntimeError("Max charge current raw must be 100..10000, i.e. 1.00..100.00 A")
        voltage_values = []
        for reg, label, key in EPEVER_VOLTAGE_ORDER:
            raw = volts_to_epever_raw(self.epever_voltage_vars[key].get().strip())
            if not 900 <= raw <= 1700:
                raise RuntimeError(f"0x{reg} {label} must be within 9.00..17.00 V for a 12V system")
            voltage_values.append(raw)
        # EPEVER has two different low-voltage chains:
        # - load disconnect/reconnect: 900A, 900D, 900E
        # - undervoltage alarm/recovery: 900B, 900C
        # Older app versions incorrectly forced 900A >= 900B. The user's safe
        # config.h values intentionally use 900B=12.70 and 900A=12.60, so do
        # not block that. We only block truly dangerous ordering mistakes.
        ovd, cl, ovr, eq, boost, flt, br, lvr, uvr, uvw, lvd, dlim = voltage_values
        warnings: List[str] = []
        if not (ovd >= cl >= ovr):
            raise RuntimeError("Unsafe high-voltage order: require OVD >= ChargingLimit >= OVR")
        if not (cl >= eq >= flt and cl >= boost >= flt):
            raise RuntimeError("Unsafe charge voltage order: require ChargingLimit >= Equalization/Boost >= Float")
        if not (flt >= br):
            warnings.append("Float is lower than Bulk/Boost Recovery. Usually Float >= BulkRecovery; check if this is intentional.")
        if not (lvr >= lvd >= dlim):
            raise RuntimeError("Unsafe load voltage order: require LowVoltageRecovery >= LowVoltageDisconnect >= DischargingLimit")
        if not (uvr >= uvw):
            raise RuntimeError("Unsafe undervoltage alarm order: require UndervoltageAlarmRecovery >= UndervoltageAlarm")
        if uvw < lvd:
            warnings.append("Undervoltage alarm is below low-voltage disconnect. This is unusual; check battery settings.")
        return {
            "battery_type": 0,
            "capacity_ah": capacity,
            "rated_voltage_level": rated_voltage,
            "max_charge_current_raw": max_charge_raw,
            "voltage_values": voltage_values,
            "warnings": warnings,
        }

    def epever_check_write_confirmations(self) -> None:
        if not self.epever_confirm_single_var.get():
            raise RuntimeError("Confirm that only this EPEVER controller is connected")
        if not self.epever_confirm_battery_var.get():
            raise RuntimeError("Confirm battery type and voltage values")
        if not self.epever_confirm_backup_var.get():
            raise RuntimeError("Confirm settings backup/readback condition")
        if not self.epever_confirm_risk_var.get():
            raise RuntimeError("Confirm write risk")

    def set_epever_busy(self, busy: bool, message: str) -> None:
        self.epever_status_var.set(message)
        self.action_status_var.set(message)
        self.set_busy_indicator(busy)
        for name in ("epever_read_button", "epever_find_id_button", "epever_change_id_button", "epever_write_all_button", "epever_write_voltage_button", "epever_write_current_button"):
            if hasattr(self, name):
                getattr(self, name).configure(state=("disabled" if busy else "normal"))

    def epever_read_config_clicked(self) -> None:
        try:
            config = self.get_epever_common_config()
        except Exception as exc:
            self.show_error("EPEVER read setup", str(exc))
            return
        self.baud_var.set("115200")
        self.epever_text.insert("end", f"\n--- EPEVER read config started at {now_log_time()} ---\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER: reading config...")
        threading.Thread(target=self.epever_read_config_worker, args=(config,), daemon=True).start()

    def epever_write_all_clicked(self) -> None:
        try:
            self.epever_check_write_confirmations()
            config = self.get_epever_common_config()
            values = self.get_epever_selected_values()
            mode = "all"
        except Exception as exc:
            if hasattr(self, "epever_text"):
                self.epever_text.insert("end", f"\n--- EPEVER WRITE ALL setup error at {now_log_time()} ---\nERROR: {exc}\n")
                self.epever_text.see("end")
            self.show_error("EPEVER write setup", str(exc))
            return
        self.baud_var.set("115200")
        self.epever_text.insert("end", f"\n--- EPEVER WRITE ALL started at {now_log_time()} ---\n")
        for warning in values.get("warnings", []):
            self.epever_text.insert("end", f"WARNING: {warning}\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER: writing selected config...")
        threading.Thread(target=self.epever_write_worker, args=(config, values, mode), daemon=True).start()

    def epever_write_voltage_clicked(self) -> None:
        try:
            self.epever_check_write_confirmations()
            config = self.get_epever_common_config()
            values = self.get_epever_selected_values()
            mode = "voltage"
        except Exception as exc:
            if hasattr(self, "epever_text"):
                self.epever_text.insert("end", f"\n--- EPEVER voltage write setup error at {now_log_time()} ---\nERROR: {exc}\n")
                self.epever_text.see("end")
            self.show_error("EPEVER voltage write setup", str(exc))
            return
        self.epever_text.insert("end", f"\n--- EPEVER WRITE VOLTAGE started at {now_log_time()} ---\n")
        for warning in values.get("warnings", []):
            self.epever_text.insert("end", f"WARNING: {warning}\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER: writing voltage block...")
        threading.Thread(target=self.epever_write_worker, args=(config, values, mode), daemon=True).start()

    def epever_write_current_clicked(self) -> None:
        try:
            self.epever_check_write_confirmations()
            config = self.get_epever_common_config()
            values = self.get_epever_selected_values()
            mode = "current"
        except Exception as exc:
            if hasattr(self, "epever_text"):
                self.epever_text.insert("end", f"\n--- EPEVER current write setup error at {now_log_time()} ---\nERROR: {exc}\n")
                self.epever_text.see("end")
            self.show_error("EPEVER current write setup", str(exc))
            return
        self.epever_text.insert("end", f"\n--- EPEVER WRITE CURRENT started at {now_log_time()} ---\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER: writing max charge current...")
        threading.Thread(target=self.epever_write_worker, args=(config, values, mode), daemon=True).start()

    def epever_check_id_change_confirmations(self) -> None:
        if not self.epever_confirm_single_var.get():
            raise RuntimeError("Confirm that only this EPEVER controller is connected")
        if not self.epever_confirm_experimental_id_var.get():
            raise RuntimeError("Confirm the custom EPEVER 0x45 ID-change command risk")

    def epever_find_id_clicked(self) -> None:
        try:
            config = self.get_epever_common_config()
        except Exception as exc:
            self.show_error("EPEVER ID find setup", str(exc))
            return
        self.baud_var.set("115200")
        self.epever_text.insert("end", f"\n--- EPEVER custom FIND/READ ID started at {now_log_time()} ---\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER ID: sending custom find/read command...")
        threading.Thread(target=self.epever_find_id_worker, args=(config,), daemon=True).start()

    def epever_change_id_clicked(self) -> None:
        try:
            self.epever_check_id_change_confirmations()
            config = self.get_epever_common_config()
            old_addr = int(config["address"])
            new_addr = parse_address(self.epever_target_addr_var.get())
            if not 1 <= new_addr <= 247:
                raise RuntimeError("New EPEVER address must be 1..247")
            if old_addr == new_addr:
                raise RuntimeError("Current and new EPEVER address are the same")
        except Exception as exc:
            self.show_error("EPEVER ID change setup", str(exc))
            return
        self.baud_var.set("115200")
        self.epever_text.insert("end", f"\n--- EPEVER custom CHANGE ID {format_address(old_addr)} → {format_address(new_addr)} started at {now_log_time()} ---\n")
        self.epever_text.insert("end", "This uses F8 45 00 01 01 NN + Modbus CRC. It is not FC06/FC10. Only one controller must be connected.\n")
        self.epever_text.see("end")
        self.set_epever_busy(True, "EPEVER ID: changing address with custom 0x45 frame...")
        threading.Thread(target=self.epever_change_id_worker, args=(config, new_addr), daemon=True).start()

    def epever_find_id_worker(self, config: Dict[str, Any]) -> None:
        try:
            req_no_crc = build_epever_service_find_id_request_no_crc()
            req = append_crc(req_no_crc)
            self.call_ui(self.set_operation_progress, 30, "EPEVER ID: TX custom find/read ID")
            self.call_ui(self.append_text_widget, self.epever_text, f"TX no CRC: {bytes_to_hex(req_no_crc)}\nTX full:   {bytes_to_hex(req)}\n")
            res = self.transport.transaction(
                port_display=str(config["port"]),
                baud=int(config["baud"]),
                request=req,
                expected_prefix=None,
                expected_length=None,
                timeout_ms=int(config["timeout"]),
                retries=int(config["retries"]),
                post_delay_ms=int(config["post_delay"]),
            )
            lines = [
                "EPEVER CUSTOM FIND/READ ID",
                f"PORT: {SerialTransport.normalize_port(str(config['port']))}",
                f"BAUD: {config['baud']}",
                f"TX full: {bytes_to_hex(req)}",
                f"RX raw:  {bytes_to_hex(res.get('rx', b''))}",
                f"RX ascii:{decode_ascii_preview(res.get('rx', b''))}",
                f"Status:  {res.get('status')}",
                f"Elapsed: {res.get('elapsed_ms')} ms",
            ]
            self.call_ui(self.append_text_widget, self.epever_text, "\n".join(lines) + "\n")
            self.call_ui(self.log_block, "EPEVER FIND/READ ID", lines)
            self.call_ui(self.set_operation_progress, 100, "EPEVER ID find/read complete")
            self.call_ui(self.set_epever_busy, False, "EPEVER ID find/read complete")
        except Exception as exc:
            self.call_ui(self.set_epever_busy, False, "EPEVER ID find/read failed")
            self.call_ui(self.show_error, "EPEVER ID find/read error", str(exc))

    def epever_change_id_worker(self, config: Dict[str, Any], new_addr: int) -> None:
        try:
            profile = self.registry.by_id("epever_solar_controller")
            if not profile:
                raise RuntimeError("EPEVER profile not found")
            old_addr = int(config["address"])
            port = str(config["port"])
            timeout = int(config["timeout"])
            post_delay = int(config["post_delay"])

            self.call_ui(self.set_operation_progress, 10, "EPEVER ID: probing old address")
            old_before = self.safe_probe(profile, old_addr, port=port, timeout=timeout, post_delay=post_delay)
            self.call_ui(self.append_text_widget, self.epever_text, f"Old address probe {format_address(old_addr)}: {old_before.get('status')} | RX {bytes_to_hex(old_before.get('rx', b''))}\n")

            find_no_crc = build_epever_service_find_id_request_no_crc()
            find_req = append_crc(find_no_crc)
            self.call_ui(self.set_operation_progress, 25, "EPEVER ID: sending find/read ID")
            self.call_ui(self.append_text_widget, self.epever_text, f"\nFIND/READ ID TX full: {bytes_to_hex(find_req)}\n")
            find_res = self.transport.transaction(
                port_display=port, baud=int(config["baud"]), request=find_req, expected_prefix=None, expected_length=None,
                timeout_ms=timeout, retries=int(config["retries"]), post_delay_ms=post_delay,
            )
            self.call_ui(self.append_text_widget, self.epever_text, f"FIND/READ ID RX raw:  {bytes_to_hex(find_res.get('rx', b''))} | {find_res.get('status')}\n")

            req_no_crc = build_epever_service_set_id_request_no_crc(new_addr)
            req = append_crc(req_no_crc)
            self.call_ui(self.set_operation_progress, 45, "EPEVER ID: sending set ID")
            self.call_ui(self.append_text_widget, self.epever_text, f"\nSET ID TX no CRC: {bytes_to_hex(req_no_crc)}\nSET ID TX full:   {bytes_to_hex(req)}\n")
            write_res = self.transport.transaction(
                port_display=port, baud=int(config["baud"]), request=req, expected_prefix=None, expected_length=None,
                timeout_ms=timeout, retries=int(config["retries"]), post_delay_ms=post_delay,
            )
            self.call_ui(self.append_text_widget, self.epever_text, f"SET ID RX raw:    {bytes_to_hex(write_res.get('rx', b''))} | {write_res.get('status')} | {write_res.get('elapsed_ms')} ms\n")

            self.call_ui(self.set_operation_progress, 65, "EPEVER ID: waiting before verification")
            time.sleep(0.8)
            self.call_ui(self.set_operation_progress, 80, "EPEVER ID: verifying new address")
            verify_new = self.safe_probe(profile, new_addr, port=port, timeout=timeout, post_delay=post_delay)
            self.call_ui(self.append_text_widget, self.epever_text, f"Verify NEW {format_address(new_addr)}: {verify_new.get('status')} | RX {bytes_to_hex(verify_new.get('rx', b''))}\n")
            self.call_ui(self.set_operation_progress, 92, "EPEVER ID: verifying old address")
            verify_old = self.safe_probe(profile, old_addr, port=port, timeout=timeout, post_delay=post_delay)
            self.call_ui(self.append_text_widget, self.epever_text, f"Verify OLD {format_address(old_addr)}: {verify_old.get('status')} | RX {bytes_to_hex(verify_old.get('rx', b''))}\n")

            if verify_new.get("ok") and not verify_old.get("ok"):
                final = "SUCCESS: new address responds, old address does not respond"
            elif verify_new.get("ok") and verify_old.get("ok"):
                final = "DANGEROUS/PARTIAL: both old and new addresses respond"
            elif write_res.get("ok") and not verify_new.get("ok"):
                final = "PARTIAL: custom set-ID returned something, but new address did not respond"
            else:
                final = "FAILED: new address did not respond"

            lines = [
                "EPEVER CUSTOM CHANGE ID",
                f"OLD: {format_address(old_addr)}",
                f"NEW: {format_address(new_addr)}",
                f"FIND TX: {bytes_to_hex(find_req)}",
                f"FIND RX: {bytes_to_hex(find_res.get('rx', b''))}",
                f"SET TX:  {bytes_to_hex(req)}",
                f"SET RX:  {bytes_to_hex(write_res.get('rx', b''))}",
                f"VERIFY NEW: {verify_new.get('status')}",
                f"VERIFY OLD: {verify_old.get('status')}",
                f"FINAL: {final}",
            ]
            self.call_ui(self.append_text_widget, self.epever_text, "\n" + "\n".join(lines) + "\n")
            self.call_ui(self.log_block, "EPEVER CHANGE ID", lines)
            self.call_ui(self.set_operation_progress, 100, "EPEVER ID change complete")
            self.call_ui(self.set_epever_busy, False, "EPEVER ID change complete: " + final)
        except Exception as exc:
            self.call_ui(self.set_epever_busy, False, "EPEVER ID change failed")
            self.call_ui(self.show_error, "EPEVER ID change error", str(exc))

    def epever_fc10_transaction(self, config: Dict[str, Any], start_register: int, values: List[int], label: str) -> Dict[str, Any]:
        req_no_crc = build_fc10(int(config["address"]), start_register, values)
        req = append_crc(req_no_crc)
        expected_prefix = bytes([int(config["address"]) & 0xFF, 0x10]) + u16_to_bytes(start_register) + u16_to_bytes(len(values))
        self.call_ui(self.append_text_widget, self.epever_text, f"\nWRITE {label}\nTX no CRC: {bytes_to_hex(req_no_crc)}\nTX full:   {bytes_to_hex(req)}\n")
        result = self.transport.transaction(
            port_display=str(config["port"]),
            baud=int(config["baud"]),
            request=req,
            expected_prefix=expected_prefix,
            expected_length=8,
            timeout_ms=int(config["timeout"]),
            retries=int(config["retries"]),
            post_delay_ms=int(config["post_delay"]),
        )
        self.call_ui(self.append_text_widget, self.epever_text, f"RX raw:    {bytes_to_hex(result.get('rx', b''))}\nFrame:     {bytes_to_hex(result.get('frame', b'') or b'')}\nStatus:    {result.get('status')} | elapsed={result.get('elapsed_ms')} ms\n")
        return {"label": label, "tx_full": bytes_to_hex(req), "rx_raw": bytes_to_hex(result.get("rx", b"")), "ok": bool(result.get("ok")), "status": result.get("status")}

    def epever_fc03_read(self, config: Dict[str, Any], start_register: int, count: int, label: str) -> Dict[str, Any]:
        req_no_crc = build_fc03(int(config["address"]), start_register, count)
        req = append_crc(req_no_crc)
        expected_prefix = bytes([int(config["address"]) & 0xFF, 0x03, count * 2])
        self.call_ui(self.append_text_widget, self.epever_text, f"\nREAD {label}\nTX full:   {bytes_to_hex(req)}\n")
        result = self.transport.transaction(
            port_display=str(config["port"]),
            baud=int(config["baud"]),
            request=req,
            expected_prefix=expected_prefix,
            expected_length=5 + 2 * count,
            timeout_ms=int(config["timeout"]),
            retries=int(config["retries"]),
            post_delay_ms=int(config["post_delay"]),
        )
        regs: List[int] = []
        frame = result.get("frame")
        if frame:
            try:
                regs = parse_registers_from_fc03_fc04_frame(frame)
            except Exception:
                regs = []
        self.call_ui(self.append_text_widget, self.epever_text, f"RX raw:    {bytes_to_hex(result.get('rx', b''))}\nFrame:     {bytes_to_hex(frame or b'')}\nStatus:    {result.get('status')} | elapsed={result.get('elapsed_ms')} ms\nRegisters: {regs}\n")
        return {"label": label, "ok": bool(result.get("ok")), "status": result.get("status"), "registers": regs, "rx_raw": bytes_to_hex(result.get("rx", b""))}

    def epever_read_config_worker(self, config: Dict[str, Any]) -> None:
        try:
            self.call_ui(self.set_operation_progress, 10, "EPEVER: reading 0x9000..0x9002")
            r1 = self.epever_fc03_read(config, 0x9000, 3, "battery type/capacity/temp compensation 0x9000..0x9002")
            self.call_ui(self.set_operation_progress, 35, "EPEVER: reading voltage block")
            r2 = self.epever_fc03_read(config, 0x9003, 12, "voltage block 0x9003..0x900E")
            self.call_ui(self.set_operation_progress, 65, "EPEVER: reading rated voltage")
            r3 = self.epever_fc03_read(config, 0x9067, 1, "rated voltage level 0x9067")
            self.call_ui(self.set_operation_progress, 85, "EPEVER: reading max charge current")
            r4 = self.epever_fc03_read(config, 0x90BF, 1, "max charge current 0x90BF")
            self.call_ui(self.epever_show_read_summary, [r1, r2, r3, r4])
        except Exception as exc:
            self.call_ui(self.set_epever_busy, False, "EPEVER read failed")
            self.call_ui(self.show_error, "EPEVER read error", str(exc))

    def epever_show_read_summary(self, results: List[Dict[str, Any]]) -> None:
        lines = ["\nREAD SUMMARY:"]
        for res in results:
            lines.append(f"- {res.get('label')}: {'OK' if res.get('ok') else 'FAIL'} / {res.get('status')}")
        self.append_text_widget(self.epever_text, "\n".join(lines) + "\n")
        self.set_epever_busy(False, "EPEVER read complete")
        self.log_block("EPEVER READ CONFIG", lines)

    def epever_write_worker(self, config: Dict[str, Any], values: Dict[str, Any], mode: str) -> None:
        steps = []
        try:
            if mode == "all":
                self.call_ui(self.set_operation_progress, 10, "EPEVER: writing battery type User")
                steps.append(self.epever_fc10_transaction(config, 0x9000, [int(values["battery_type"])], "Battery type 0x9000 = User(0)"))
                self.call_ui(self.set_operation_progress, 20, "EPEVER: writing capacity")
                steps.append(self.epever_fc10_transaction(config, 0x9001, [int(values["capacity_ah"])], "Battery capacity 0x9001"))
                self.call_ui(self.set_operation_progress, 30, "EPEVER: writing rated voltage level")
                steps.append(self.epever_fc10_transaction(config, 0x9067, [int(values["rated_voltage_level"])], "Rated voltage level 0x9067"))
            if mode in ("all", "voltage"):
                self.call_ui(self.set_operation_progress, 45, "EPEVER: writing voltage block 0x9003..0x900E")
                steps.append(self.epever_fc10_transaction(config, 0x9003, list(values["voltage_values"]), "Voltage block 0x9003..0x900E"))
            if mode in ("all", "current"):
                self.call_ui(self.set_operation_progress, 65, "EPEVER: writing max charge current 0x90BF")
                steps.append(self.epever_fc10_transaction(config, 0x90BF, [int(values["max_charge_current_raw"])], "Max charge current 0x90BF"))
            self.call_ui(self.set_operation_progress, 80, "EPEVER: verifying written values")
            verify = [
                self.epever_fc03_read(config, 0x9000, 3, "verify 0x9000..0x9002"),
                self.epever_fc03_read(config, 0x9003, 12, "verify voltage block"),
                self.epever_fc03_read(config, 0x9067, 1, "verify rated voltage"),
                self.epever_fc03_read(config, 0x90BF, 1, "verify max charge current"),
            ]
            ok = all(s.get("ok") for s in steps) and all(v.get("ok") for v in verify)
            self.call_ui(self.epever_show_write_summary, steps, verify, ok)
        except Exception as exc:
            self.call_ui(self.set_epever_busy, False, "EPEVER write failed")
            self.call_ui(self.show_error, "EPEVER write error", str(exc))

    def epever_show_write_summary(self, steps: List[Dict[str, Any]], verify: List[Dict[str, Any]], ok: bool) -> None:
        lines = ["\nWRITE SUMMARY:"]
        for step in steps:
            lines.append(f"- WRITE {step.get('label')}: {'OK' if step.get('ok') else 'FAIL'} / {step.get('status')}")
        for item in verify:
            lines.append(f"- VERIFY {item.get('label')}: {'OK' if item.get('ok') else 'FAIL'} / {item.get('status')}")
        lines.append("FINAL: " + ("SUCCESS" if ok else "FAILED/PARTIAL - check TX/RX above"))
        self.append_text_widget(self.epever_text, "\n".join(lines) + "\n")
        self.set_epever_busy(False, "EPEVER write complete: " + ("SUCCESS" if ok else "FAILED/PARTIAL"))
        self.log_block("EPEVER WRITE CONFIG", lines)

    # ------------------------------------------------------------------
    # Export and copy
    # ------------------------------------------------------------------

    def export_scan_csv(self) -> None:
        if not self.last_records:
            self.show_error("Export", "No records to export")
            return
        path = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV files", "*.csv"), ("All files", "*.*")])
        if not path:
            return
        flat_rows = self.flatten_records(self.last_records)
        with open(path, "w", encoding="utf-8-sig", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=sorted({k for row in flat_rows for k in row.keys()}))
            writer.writeheader()
            writer.writerows(flat_rows)
        self.log(f"CSV exported: {path}")

    def export_scan_json(self) -> None:
        if not self.last_records:
            self.show_error("Export", "No records to export")
            return
        path = filedialog.asksaveasfilename(defaultextension=".json", filetypes=[("JSON files", "*.json"), ("All files", "*.*")])
        if not path:
            return
        data = {
            "app": APP_NAME,
            "version": APP_VERSION,
            "exported_at": now_local_iso(),
            "records": self.last_records,
        }
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        self.log(f"JSON exported: {path}")

    @staticmethod
    def flatten_records(records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        rows: List[Dict[str, Any]] = []
        for record in records:
            base = {k: v for k, v in record.items() if k not in ("values", "steps")}
            if "values" in record and record["values"]:
                for value in record["values"]:
                    row = dict(base)
                    row.update({
                        "field_name": value.get("name"),
                        "field_label": value.get("label"),
                        "field_value": value.get("value"),
                        "field_unit": value.get("unit"),
                        "field_raw": value.get("raw"),
                        "field_status": value.get("status"),
                    })
                    rows.append(row)
            else:
                rows.append(base)
        return rows

    def copy_log_selected(self) -> None:
        try:
            selected = self.log_text.get("sel.first", "sel.last")
        except Exception:
            selected = ""
        if selected:
            self.clipboard_clear()
            self.clipboard_append(selected)

    def copy_log_all(self) -> None:
        text = self.log_text.get("1.0", "end-1c")
        self.clipboard_clear()
        self.clipboard_append(text)

    def save_log(self) -> None:
        path = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if not path:
            return
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.log_text.get("1.0", "end-1c"))
        self.log(f"Log saved: {path}")

    # ------------------------------------------------------------------
    # Closing
    # ------------------------------------------------------------------

    def on_close(self) -> None:
        try:
            self.scan_stop_event.set()
            self.continuous_stop_event.set()
            self.transport.close()
        finally:
            self.destroy()


# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------

def main() -> int:
    try:
        app = UsbRs485SensorTool()
        app.mainloop()
        return 0
    except Exception as exc:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(APP_TITLE, str(exc))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
