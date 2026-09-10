@echo off
setlocal
cd /d "%~dp0"
if exist "%~dp0USB_RS485_Sensor_Tool.exe" (
  start "" "%~dp0USB_RS485_Sensor_Tool.exe"
  exit /b 0
)
if exist "%~dp0dist\USB_RS485_Sensor_Tool.exe" (
  start "" "%~dp0dist\USB_RS485_Sensor_Tool.exe"
  exit /b 0
)
py -3 app.py
if errorlevel 1 (
  echo.
  echo Application exited with an error.
  pause
)
