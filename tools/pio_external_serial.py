# -*- coding: utf-8 -*-
"""
PlatformIO extra script that adds two custom targets:
- serial_gui: opens the external serial monitor window.
- upload_and_serial_gui: uploads firmware, then opens the external serial monitor window.
"""

from __future__ import annotations

import os
import subprocess
import sys

Import("env")  # type: ignore[name-defined]


def open_external_serial_monitor(source, target, env):  # noqa: ANN001
    project_dir = env.subst("$PROJECT_DIR")
    pio_env = env.subst("$PIOENV")
    script_path = os.path.join(project_dir, "tools", "serial_monitor.py")

    if not os.path.exists(script_path):
        print("External Serial Monitor was not found:", script_path)
        return

    args = [
        sys.executable,
        script_path,
        "--project",
        project_dir,
        "--env",
        pio_env,
    ]

    creationflags = 0
    if os.name == "nt":
        creationflags = getattr(subprocess, "CREATE_NEW_CONSOLE", 0)

    subprocess.Popen(
        args,
        cwd=project_dir,
        creationflags=creationflags,
        close_fds=(os.name != "nt"),
    )


env.AddCustomTarget(
    name="serial_gui",
    dependencies=None,
    actions=[open_external_serial_monitor],
    title="Serial GUI",
    description="Open external serial monitor window",
)

env.AddCustomTarget(
    name="upload_and_serial_gui",
    dependencies=["upload"],
    actions=[open_external_serial_monitor],
    title="Upload + Serial GUI",
    description="Upload firmware and then open external serial monitor window",
)
