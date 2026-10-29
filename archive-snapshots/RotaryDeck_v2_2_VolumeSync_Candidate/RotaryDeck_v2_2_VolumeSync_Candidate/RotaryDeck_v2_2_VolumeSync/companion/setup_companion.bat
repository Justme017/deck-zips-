@echo off
cd /d "%~dp0"
py -3 -m venv .venv
if errorlevel 1 goto fail
".venv\Scripts\python.exe" -m pip install --upgrade pip
if errorlevel 1 goto fail
".venv\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto fail
".venv\Scripts\python.exe" rotary_deck_sync.py --diagnose
if errorlevel 1 goto fail
echo Setup and basic diagnostics completed. Run start_companion.bat next.
pause
exit /b 0
:fail
echo Setup or diagnostics failed. Copy the error above; do not reset pairing yet.
pause
exit /b 1
