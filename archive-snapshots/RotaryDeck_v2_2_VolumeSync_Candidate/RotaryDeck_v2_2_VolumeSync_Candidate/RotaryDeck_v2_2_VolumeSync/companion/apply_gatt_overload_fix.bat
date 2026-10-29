@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo Put both patch files in your current companion folder beside rotary_deck_sync.py and .venv.
 pause
 exit /b 1
)
".venv\Scripts\python.exe" fix_gatt_overloads.py
pause
