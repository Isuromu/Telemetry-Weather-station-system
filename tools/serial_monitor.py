#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
External Serial Monitor for PlatformIO + VS Code.
Works as a separate window, similar in spirit to the old Arduino IDE Serial Monitor.

Features:
- Reads monitor_port / upload_port / monitor_speed from platformio.ini.
- Can be launched from PlatformIO custom targets or VS Code tasks.
- Has port refresh, connect/disconnect, clear, send line, newline selection.
- Does not use VS Code integrated terminal for serial output.
"""

from __future__ import annotations

import argparse
import configparser
import os
import queue
import re
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import tkinter as tk
    from tkinter import messagebox, ttk
except Exception as exc:  # pragma: no cover - GUI import failure path
    print("ERROR: tkinter is not available in this Python installation.", file=sys.stderr)
    print(str(exc), file=sys.stderr)
    sys.exit(1)

try:
    import serial
    from serial.tools import list_ports
except Exception as exc:  # pragma: no cover - dependency failure path
    root = tk.Tk()
    root.withdraw()
    messagebox.showerror(
        "Missing dependency: pyserial",
        "Python package 'pyserial' is not installed.\n\n"
        "Install it with:\n"
        "python -m pip install pyserial\n\n"
        f"Original error:\n{exc}",
    )
    sys.exit(1)


@dataclass
class PioConfig:
    project_dir: Path
    env_name: str
    monitor_port: Optional[str]
    upload_port: Optional[str]
    monitor_speed: int


def _split_platformio_list(value: str) -> List[str]:
    value = value.replace(",", " ").replace("\n", " ")
    return [part.strip() for part in value.split() if part.strip()]


def _clean_ini_value(value: Optional[str]) -> Optional[str]:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    # PlatformIO values sometimes contain accidental comments when copied manually.
    text = re.split(r"\s[;#]", text, maxsplit=1)[0].strip()
    return text if text else None


def _read_platformio_ini(project_dir: Path) -> configparser.ConfigParser:
    ini_path = project_dir / "platformio.ini"
    parser = configparser.ConfigParser(
        interpolation=None,
        inline_comment_prefixes=(";", "#"),
        strict=False,
    )
    parser.optionxform = str
    if ini_path.exists():
        with ini_path.open("r", encoding="utf-8", errors="replace") as f:
            parser.read_file(f)
    return parser


def _find_env_name(parser: configparser.ConfigParser, requested_env: Optional[str]) -> str:
    if requested_env:
        return requested_env.strip()

    env_from_os = os.environ.get("PIOENV", "").strip()
    if env_from_os:
        return env_from_os

    if parser.has_section("platformio") and parser.has_option("platformio", "default_envs"):
        envs = _split_platformio_list(parser.get("platformio", "default_envs", fallback=""))
        if envs:
            return envs[0]

    env_sections = [section[4:] for section in parser.sections() if section.startswith("env:")]
    if env_sections:
        return env_sections[0]

    return ""


def _get_option(
    parser: configparser.ConfigParser,
    env_name: str,
    option_name: str,
) -> Optional[str]:
    # First check exact environment, then shared [env] section.
    env_section = f"env:{env_name}" if env_name else ""
    if env_section and parser.has_section(env_section) and parser.has_option(env_section, option_name):
        return _clean_ini_value(parser.get(env_section, option_name, fallback=None))
    if parser.has_section("env") and parser.has_option("env", option_name):
        return _clean_ini_value(parser.get("env", option_name, fallback=None))
    return None


def load_pio_config(project_dir: Path, requested_env: Optional[str]) -> PioConfig:
    parser = _read_platformio_ini(project_dir)
    env_name = _find_env_name(parser, requested_env)

    monitor_port = _get_option(parser, env_name, "monitor_port")
    upload_port = _get_option(parser, env_name, "upload_port")

    speed_text = _get_option(parser, env_name, "monitor_speed")
    monitor_speed = 115200
    if speed_text:
        try:
            monitor_speed = int(speed_text)
        except ValueError:
            monitor_speed = 115200

    return PioConfig(
        project_dir=project_dir,
        env_name=env_name,
        monitor_port=monitor_port,
        upload_port=upload_port,
        monitor_speed=monitor_speed,
    )


def list_serial_ports() -> List[Tuple[str, str]]:
    ports: List[Tuple[str, str]] = []
    for port in list_ports.comports():
        description = port.description or "Serial device"
        hwid = port.hwid or ""
        label = f"{port.device} — {description}"
        if hwid and hwid != "n/a":
            label += f" [{hwid}]"
        ports.append((port.device, label))
    ports.sort(key=lambda item: item[0].lower())
    return ports


def choose_default_port(config: PioConfig, available_ports: List[Tuple[str, str]]) -> str:
    available_names = [device for device, _label in available_ports]

    for candidate in (config.monitor_port, config.upload_port):
        if candidate and candidate.lower() not in ("auto", "none"):
            if candidate in available_names:
                return candidate
            return candidate

    # Prefer USB serial adapters and avoid Bluetooth ports when possible.
    for device, label in available_ports:
        low = label.lower()
        if "bluetooth" in low:
            continue
        if any(token in low for token in ("usb", "ch340", "ch343", "cp210", "ftdi", "uart", "serial")):
            return device

    return available_ports[0][0] if available_ports else ""


class SerialMonitorApp(tk.Tk):
    def __init__(self, config: PioConfig, forced_port: Optional[str], forced_baud: Optional[int]) -> None:
        super().__init__()
        self.config = config
        self.title("External Serial Monitor - PlatformIO")
        self.geometry("980x620")
        self.minsize(760, 420)

        self.serial_conn: Optional[serial.Serial] = None
        self.reader_thread: Optional[threading.Thread] = None
        self.reader_alive = threading.Event()
        self.rx_queue: "queue.Queue[Tuple[str, str]]" = queue.Queue()
        self.line_start = True

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(forced_baud or config.monitor_speed or 115200))
        self.status_var = tk.StringVar(value="Disconnected")
        self.newline_var = tk.StringVar(value="LF")
        self.autoscroll_var = tk.BooleanVar(value=True)
        self.timestamp_var = tk.BooleanVar(value=False)
        self.dtr_var = tk.BooleanVar(value=False)
        self.rts_var = tk.BooleanVar(value=False)
        self.encoding_var = tk.StringVar(value="utf-8")

        self.available_ports: List[Tuple[str, str]] = []
        self._build_ui()
        self.refresh_ports(select_port=forced_port)
        self.after(30, self._process_rx_queue)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=8)
        root.pack(fill=tk.BOTH, expand=True)

        top = ttk.Frame(root)
        top.pack(fill=tk.X)

        ttk.Label(top, text="Port:").pack(side=tk.LEFT, padx=(0, 4))
        self.port_combo = ttk.Combobox(top, textvariable=self.port_var, width=42, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=(0, 8))

        ttk.Button(top, text="Refresh", command=self.refresh_ports).pack(side=tk.LEFT, padx=(0, 8))

        ttk.Label(top, text="Baud:").pack(side=tk.LEFT, padx=(0, 4))
        self.baud_combo = ttk.Combobox(
            top,
            textvariable=self.baud_var,
            width=10,
            values=("9600", "19200", "38400", "57600", "74880", "115200", "230400", "460800", "921600"),
        )
        self.baud_combo.pack(side=tk.LEFT, padx=(0, 8))

        self.connect_button = ttk.Button(top, text="Connect", command=self.connect_or_disconnect)
        self.connect_button.pack(side=tk.LEFT, padx=(0, 8))

        ttk.Button(top, text="Clear", command=self.clear_output).pack(side=tk.LEFT, padx=(0, 8))

        ttk.Checkbutton(top, text="Auto-scroll", variable=self.autoscroll_var).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Checkbutton(top, text="Timestamp", variable=self.timestamp_var).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Checkbutton(top, text="DTR", variable=self.dtr_var, command=self.apply_control_lines).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Checkbutton(top, text="RTS", variable=self.rts_var, command=self.apply_control_lines).pack(side=tk.LEFT, padx=(0, 8))

        info = ttk.Frame(root)
        info.pack(fill=tk.X, pady=(6, 6))
        env_text = self.config.env_name if self.config.env_name else "not detected"
        ttk.Label(info, text=f"Project: {self.config.project_dir}").pack(side=tk.LEFT)
        ttk.Label(info, text=f"    Env: {env_text}").pack(side=tk.LEFT)

        text_frame = ttk.Frame(root)
        text_frame.pack(fill=tk.BOTH, expand=True)

        self.output_text = tk.Text(
            text_frame,
            wrap=tk.NONE,
            font=("Consolas", 10),
            undo=False,
            maxundo=0,
        )
        self.output_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        y_scroll = ttk.Scrollbar(text_frame, orient=tk.VERTICAL, command=self.output_text.yview)
        y_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.output_text.configure(yscrollcommand=y_scroll.set)

        x_scroll = ttk.Scrollbar(root, orient=tk.HORIZONTAL, command=self.output_text.xview)
        x_scroll.pack(fill=tk.X)
        self.output_text.configure(xscrollcommand=x_scroll.set)

        bottom = ttk.Frame(root)
        bottom.pack(fill=tk.X, pady=(8, 0))

        ttk.Label(bottom, text="Send:").pack(side=tk.LEFT, padx=(0, 4))
        self.input_var = tk.StringVar()
        self.input_entry = ttk.Entry(bottom, textvariable=self.input_var)
        self.input_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))
        self.input_entry.bind("<Return>", lambda _event: self.send_text())

        ttk.Label(bottom, text="Line ending:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Combobox(
            bottom,
            textvariable=self.newline_var,
            state="readonly",
            width=7,
            values=("None", "LF", "CR", "CRLF"),
        ).pack(side=tk.LEFT, padx=(0, 8))

        ttk.Button(bottom, text="Send", command=self.send_text).pack(side=tk.LEFT, padx=(0, 8))

        status_bar = ttk.Frame(root)
        status_bar.pack(fill=tk.X, pady=(8, 0))
        ttk.Label(status_bar, textvariable=self.status_var).pack(side=tk.LEFT)
        ttk.Label(status_bar, text="Encoding:").pack(side=tk.RIGHT, padx=(12, 4))
        ttk.Combobox(
            status_bar,
            textvariable=self.encoding_var,
            state="readonly",
            width=10,
            values=("utf-8", "ascii", "latin-1"),
        ).pack(side=tk.RIGHT)

    def refresh_ports(self, select_port: Optional[str] = None) -> None:
        self.available_ports = list_serial_ports()
        labels = [label for _device, label in self.available_ports]
        self.port_combo.configure(values=labels)

        selected_device = select_port or choose_default_port(self.config, self.available_ports)
        selected_label = ""
        for device, label in self.available_ports:
            if device == selected_device:
                selected_label = label
                break

        if selected_label:
            self.port_var.set(selected_label)
        elif selected_device:
            self.port_var.set(selected_device)
        elif labels:
            self.port_var.set(labels[0])
        else:
            self.port_var.set("")
            self.status_var.set("No serial ports found")

    def _selected_device(self) -> str:
        selected = self.port_var.get().strip()
        if not selected:
            return ""
        for device, label in self.available_ports:
            if selected == label or selected == device or selected.startswith(device + " "):
                return device
        if " — " in selected:
            return selected.split(" — ", 1)[0].strip()
        return selected

    def connect_or_disconnect(self) -> None:
        if self.serial_conn and self.serial_conn.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self._selected_device()
        if not port:
            messagebox.showwarning("No port", "No serial port selected.")
            return

        try:
            baud = int(self.baud_var.get().strip())
        except ValueError:
            messagebox.showerror("Invalid baud", "Baud rate must be a number, for example 115200.")
            return

        try:
            conn = serial.Serial()
            conn.port = port
            conn.baudrate = baud
            conn.timeout = 0.1
            conn.write_timeout = 1.0
            conn.dsrdtr = False
            conn.rtscts = False
            conn.dtr = bool(self.dtr_var.get())
            conn.rts = bool(self.rts_var.get())
            conn.open()
            conn.setDTR(bool(self.dtr_var.get()))
            conn.setRTS(bool(self.rts_var.get()))
        except Exception as exc:
            messagebox.showerror("Connection failed", f"Could not open {port} at {baud} baud.\n\n{exc}")
            self.status_var.set(f"Connection failed: {port}")
            return

        self.serial_conn = conn
        self.reader_alive.set()
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()

        self.connect_button.configure(text="Disconnect")
        self.status_var.set(f"Connected: {port} @ {baud}")
        self._append_text(f"\n--- Connected to {port} @ {baud} ---\n")

    def disconnect(self) -> None:
        self.reader_alive.clear()
        conn = self.serial_conn
        self.serial_conn = None
        if conn:
            try:
                if conn.is_open:
                    conn.close()
            except Exception:
                pass
        self.connect_button.configure(text="Connect")
        self.status_var.set("Disconnected")
        self._append_text("\n--- Disconnected ---\n")

    def _reader_loop(self) -> None:
        while self.reader_alive.is_set():
            conn = self.serial_conn
            if not conn or not conn.is_open:
                break
            try:
                pending = conn.in_waiting
                data = conn.read(pending if pending > 0 else 1)
                if data:
                    encoding = self.encoding_var.get() or "utf-8"
                    text = data.decode(encoding, errors="replace")
                    self.rx_queue.put(("data", text))
            except Exception as exc:
                self.rx_queue.put(("error", str(exc)))
                break

    def _process_rx_queue(self) -> None:
        try:
            while True:
                kind, text = self.rx_queue.get_nowait()
                if kind == "data":
                    self._append_text(text)
                elif kind == "error":
                    self._append_text(f"\n--- Serial error: {text} ---\n")
                    self.disconnect()
        except queue.Empty:
            pass
        self.after(30, self._process_rx_queue)

    def _append_text(self, text: str) -> None:
        self.output_text.configure(state=tk.NORMAL)
        if self.timestamp_var.get():
            self.output_text.insert(tk.END, self._timestamped_text(text))
        else:
            self.output_text.insert(tk.END, text)
        if self.autoscroll_var.get():
            self.output_text.see(tk.END)
        self.output_text.configure(state=tk.NORMAL)

    def _timestamped_text(self, text: str) -> str:
        result: List[str] = []
        for ch in text:
            if self.line_start:
                result.append(time.strftime("[%H:%M:%S] "))
                self.line_start = False
            result.append(ch)
            if ch == "\n":
                self.line_start = True
        return "".join(result)

    def clear_output(self) -> None:
        self.output_text.delete("1.0", tk.END)
        self.line_start = True

    def apply_control_lines(self) -> None:
        conn = self.serial_conn
        if conn and conn.is_open:
            try:
                conn.setDTR(bool(self.dtr_var.get()))
                conn.setRTS(bool(self.rts_var.get()))
                self.status_var.set(
                    f"Connected: {conn.port} @ {conn.baudrate} | DTR={int(self.dtr_var.get())} RTS={int(self.rts_var.get())}"
                )
            except Exception as exc:
                self._append_text(f"\n--- Control line error: {exc} ---\n")

    def send_text(self) -> None:
        conn = self.serial_conn
        if not conn or not conn.is_open:
            messagebox.showwarning("Not connected", "Serial port is not connected.")
            return

        text = self.input_var.get()
        ending = self.newline_var.get()
        suffix_map: Dict[str, str] = {
            "None": "",
            "LF": "\n",
            "CR": "\r",
            "CRLF": "\r\n",
        }
        payload = text + suffix_map.get(ending, "\n")
        try:
            conn.write(payload.encode(self.encoding_var.get() or "utf-8", errors="replace"))
            conn.flush()
            self.input_var.set("")
        except Exception as exc:
            messagebox.showerror("Send failed", str(exc))

    def on_close(self) -> None:
        self.disconnect()
        self.destroy()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="External Serial Monitor for PlatformIO projects.")
    parser.add_argument("--project", default=os.getcwd(), help="PlatformIO project directory")
    parser.add_argument("--env", default=None, help="PlatformIO environment name")
    parser.add_argument("--port", default=None, help="Serial port, for example COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=None, help="Serial baud rate, for example 115200")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_dir = Path(args.project).resolve()
    config = load_pio_config(project_dir, args.env)

    if args.baud is not None:
        config.monitor_speed = args.baud

    app = SerialMonitorApp(config=config, forced_port=args.port, forced_baud=args.baud)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
