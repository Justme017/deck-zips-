@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
 echo First run setup_companion.bat in this folder.
 pause
 exit /b 1
)
".venv\Scripts\python.exe" rotary_deck_sync.py %*
echo.
pause
