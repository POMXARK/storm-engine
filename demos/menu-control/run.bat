@echo off
rem The menu control autotest: the main menu of the interface demo driven only by messages.
rem Run it with: run.bat [engine path]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
if errorlevel 1 pause
