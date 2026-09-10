@echo off
setlocal
cd /d "%~dp0"
echo Installing requirements for USB-RS485 Sensor Tool...
py -3 --version
if errorlevel 1 (
  echo.
  echo ERROR: Python 3 was not found.
  echo Install Python from https://www.python.org/downloads/windows/
  echo During installation enable: Add Python to PATH and tcl/tk and IDLE.
  pause
  exit /b 1
)
py -3 -m pip install --upgrade pip
py -3 -m pip install -r requirements.txt
if errorlevel 1 (
  echo.
  echo ERROR: Failed to install requirements.
  pause
  exit /b 1
)
echo.
echo Requirements installed successfully.
pause
