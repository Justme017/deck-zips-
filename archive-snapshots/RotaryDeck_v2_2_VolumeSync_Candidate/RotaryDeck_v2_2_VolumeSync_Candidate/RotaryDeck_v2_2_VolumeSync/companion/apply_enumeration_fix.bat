@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo Put both patch files in the existing companion folder, beside its .venv folder.
 pause
 exit /b 1
)
".venv\Scripts\python.exe" fix_companion_enumeration.py
pause
