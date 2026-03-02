@echo off
setlocal enabledelayedexpansion
echo [fake-nuget] invoked
echo [fake-nuget] argc=%*
echo [fake-nuget] CWD=%CD%
echo [fake-nuget] ---- args ----
set i=0
:loop
if "%~1"=="" goto end
set /a i+=1
echo [fake-nuget] arg!i!=%~1
shift
goto loop
:end
echo [fake-nuget] done
exit /b 0
