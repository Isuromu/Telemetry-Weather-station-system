@echo off
setlocal
cd /d "%~dp0"
set "TARGET=%~dp0START_USB_RS485_SENSOR_TOOL.bat"
set "WORKDIR=%~dp0"
set "SHORTCUT=%USERPROFILE%\Desktop\USB RS485 Sensor Tool.lnk"
set "PS1=%TEMP%\create_usb_rs485_shortcut_%RANDOM%.ps1"
> "%PS1%" echo $WshShell = New-Object -ComObject WScript.Shell
>> "%PS1%" echo $Shortcut = $WshShell.CreateShortcut('%SHORTCUT%')
>> "%PS1%" echo $Shortcut.TargetPath = '%TARGET%'
>> "%PS1%" echo $Shortcut.WorkingDirectory = '%WORKDIR%'
>> "%PS1%" echo $Shortcut.IconLocation = 'shell32.dll,220'
>> "%PS1%" echo $Shortcut.Save()
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
set "ERR=%ERRORLEVEL%"
del "%PS1%" >nul 2>nul
if not "%ERR%"=="0" (
  echo Failed to create shortcut.
  echo Manual method: right click START_USB_RS485_SENSOR_TOOL.bat ^> Send to ^> Desktop ^(create shortcut^)
  pause
  exit /b 1
)
echo Desktop shortcut created:
echo %SHORTCUT%
pause
