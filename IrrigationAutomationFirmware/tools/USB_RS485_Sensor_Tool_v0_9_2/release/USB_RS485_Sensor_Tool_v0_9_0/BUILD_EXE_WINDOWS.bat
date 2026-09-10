@echo off
setlocal
cd /d "%~dp0"
echo Building USB_RS485_Sensor_Tool.exe using PyInstaller...
py -3 --version
if errorlevel 1 (
  echo.
  echo ERROR: Python 3 was not found.
  echo Install Python from https://www.python.org/downloads/windows/
  pause
  exit /b 1
)
py -3 -m pip install --upgrade pip
py -3 -m pip install pyserial pyinstaller
if errorlevel 1 (
  echo.
  echo ERROR: Failed to install build requirements.
  pause
  exit /b 1
)
py -3 -m PyInstaller --clean --noconfirm --onefile --windowed --name USB_RS485_Sensor_Tool --add-data "device_profiles.json;." app.py
if errorlevel 1 (
  echo.
  echo ERROR: Build failed.
  pause
  exit /b 1
)
echo.
echo Build finished.
echo EXE location:
echo %CD%\dist\USB_RS485_Sensor_Tool.exe
pause
