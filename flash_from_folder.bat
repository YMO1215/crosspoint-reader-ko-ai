@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "CHIP=esp32c3"
set "BAUD=460800"
set "STATE_FILE=%~dp0flash_from_folder.state"
set "DEFAULT_PORT=COM5"
set "LAST_DIR="
set "LAST_PORT="

if exist "%STATE_FILE%" (
  for /f "usebackq tokens=1,* delims==" %%A in ("%STATE_FILE%") do (
    if /i "%%A"=="LAST_DIR" set "LAST_DIR=%%B"
    if /i "%%A"=="LAST_PORT" set "LAST_PORT=%%B"
  )
)

if defined LAST_PORT set "DEFAULT_PORT=%LAST_PORT%"

echo Last folder: %LAST_DIR%
echo Last port: %DEFAULT_PORT%
echo.
choice /c YN /m "Reuse last folder?"
if errorlevel 2 goto pick_folder
if errorlevel 1 goto use_last_folder

:use_last_folder
if not defined LAST_DIR goto pick_folder
set "FWDIR=%LAST_DIR%"
goto folder_ready

:pick_folder
for /f "usebackq delims=" %%I in (`powershell -NoProfile -STA -Command "Add-Type -AssemblyName System.Windows.Forms; $dialog = New-Object System.Windows.Forms.FolderBrowserDialog; $dialog.Description = 'Select firmware folder'; if ('%LAST_DIR%' -ne '') { $dialog.SelectedPath = '%LAST_DIR%' }; if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { [Console]::Write($dialog.SelectedPath) }"`) do set "FWDIR=%%I"

if not defined FWDIR (
  echo No folder selected.
  exit /b 1
)

:folder_ready
echo Selected folder: %FWDIR%

if not exist "%FWDIR%\bootloader.bin" (
  echo Missing file: %FWDIR%\bootloader.bin
  exit /b 1
)
if not exist "%FWDIR%\partitions.bin" (
  echo Missing file: %FWDIR%\partitions.bin
  exit /b 1
)
if not exist "%FWDIR%\firmware.bin" (
  echo Missing file: %FWDIR%\firmware.bin
  exit /b 1
)

set /p "PORT=Enter COM port [%DEFAULT_PORT%]: "
if "%PORT%"=="" set "PORT=%DEFAULT_PORT%"

echo.
choice /c YN /m "Erase flash before writing?"
set "DO_ERASE=0"
if errorlevel 1 set "DO_ERASE=1"
if errorlevel 2 set "DO_ERASE=0"

echo.
echo Ready to flash:
echo   Chip: %CHIP%
echo   Port: %PORT%
echo   Baud: %BAUD%
echo   Folder: %FWDIR%
if "%DO_ERASE%"=="1" (
  echo   Erase: yes
) else (
  echo   Erase: no
)
echo.
choice /c YN /m "Proceed with flashing?"
if errorlevel 2 exit /b 1

if "%DO_ERASE%"=="1" (
  python -m esptool --chip %CHIP% --port %PORT% erase-flash
  if errorlevel 1 exit /b 1
)

python -m esptool --chip %CHIP% --port %PORT% --baud %BAUD% write-flash -z 0x0 "%FWDIR%\bootloader.bin" 0x8000 "%FWDIR%\partitions.bin" 0x10000 "%FWDIR%\firmware.bin"
if errorlevel 1 exit /b 1

(
  echo LAST_DIR=%FWDIR%
  echo LAST_PORT=%PORT%
) > "%STATE_FILE%"

echo.
echo Flash completed.
echo Saved last folder and port to %STATE_FILE%
pause
