@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "PIDFILE=%CD%\simulator.pid"

if exist "%PIDFILE%" (
  set /p OLD=<%PIDFILE%
)
if defined OLD (
  tasklist /FI "PID eq %OLD%" 2>nul | findstr /R /C:" %OLD% " >nul
  if not errorlevel 1 (
    echo Already running (pid %OLD%). Logs: %CD%\log.txt
    exit /b 0
  )
  del /f /q "%PIDFILE%" >nul 2>&1
)

where node >nul 2>&1
if errorlevel 1 (
  echo node is not on PATH. Install Node.js 20+ from https://nodejs.org
  exit /b 1
)

powershell -NoProfile -Command "$p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c','node server.js > log.txt 2>&1' -WorkingDirectory '%CD%' -WindowStyle Hidden -PassThru; Set-Content -LiteralPath '%CD%\simulator.pid' -Value $p.Id -NoNewline"
if errorlevel 1 (
  echo Failed to start the simulator.
  exit /b 1
)

set /p PID=<%PIDFILE%
echo Started simulator (pid %PID%). Logs: %CD%\log.txt
