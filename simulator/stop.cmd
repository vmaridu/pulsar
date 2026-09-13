@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "PIDFILE=%CD%\simulator.pid"

if not exist "%PIDFILE%" (
  echo Simulator is not running.
  exit /b 0
)

set /p PID=<%PIDFILE%
tasklist /FI "PID eq %PID%" 2>nul | findstr /R /C:" %PID% " >nul
if errorlevel 1 (
  del /f /q "%PIDFILE%" >nul 2>&1
  echo Simulator is not running.
  exit /b 0
)

taskkill /PID %PID% /T /F >nul
del /f /q "%PIDFILE%" >nul 2>&1
echo Stopped simulator (pid %PID%).
