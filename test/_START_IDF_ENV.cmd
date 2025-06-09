@REM Script for launching ESP-IDF environment on Windows
@REM Author: Paul Abbott, Lumitec, 2022
@ECHO OFF
TITLE ESP-IDF
SETLOCAL EnableDelayedExpansion

: %CD% gives current dir
SET THISDIR=%CD%
: %~dp0 gives the directory of this batch file (with a trailing \)
SET SCRIPTDIR=%~dp0

:: Check for python
(python.exe -V)>nul 2>&1 && goto :PY_INSTALLED
:: Python not found on the path
:: try some hardcoded paths
SET "PY_PATH=C:\Espressif\tools\idf-python\3.11.2"
IF EXIST %PY_PATH%\python.exe SET "PATH=%PY_PATH%;%PATH%"
:: Check again
(python.exe -V)>nul 2>&1 && goto :PY_INSTALLED  
SET "PY_PATH=C:\Espressif\tools\idf-python\3.8.7"
IF EXIST %PY_PATH%\python.exe SET "PATH=%PY_PATH%;%PATH%"
:: Check again
(python.exe -V)>nul 2>&1 && goto :PY_INSTALLED  

ECHO can't find python.exe.  Please reinstall the IDF tools 
ECHO https://github.com/espressif/idf-installer/releases/download/offline-4.3.4/esp-idf-tools-setup-offline-4.3.4.exe
ECHO https://dl.espressif.com/dl/esp-idf/
ECHO and reboot
goto :DIE

:PY_INSTALLED

:: enable ccache to greatly speed-up the build (i.e. fast subsequent builds after a clean)
SET "IDF_CCACHE_ENABLE=1"

SET "USE_IDF_INSTALLER=1"
IF %USE_IDF_INSTALLER% EQU 1 (
    GOTO :CONF_IDF_FROM_INSTALLER
) ELSE (
    GOTO :CONF_IDF_FROM_PATH
)

:CONF_IDF_FROM_INSTALLER
:: if using IDF installed globally with the idf tools installer (Windows only) https://dl.espressif.com/dl/esp-idf/
:: See C:\Espressif\esp_idf.json for installed IDF version strings
:: Version 4.4.8
SET "IDF_VERS_STRING=esp-idf-f74beb9ff6fcad0892241ba3a3d62961"
SET "IDF_INIT_SCRIPT=C:\Espressif\idf_cmd_init.bat"
IF NOT EXIST %IDF_INIT_SCRIPT% (
    :: MORE %SCRIPTDIR%readme.md
    ECHO Script %IDF_INIT_SCRIPT% does not exist!.
    GOTO :DIE
)
SET "IDF_INIT_SCRIPT=%IDF_INIT_SCRIPT% %IDF_VERS_STRING%"
GOTO :RUNSCRIPT

:CONF_IDF_FROM_PATH
:: if using a known IDF path:
SET "IDF_PATH=C:\Espressif\frameworks\esp-idf-v4.4.8"
SET "IDF_INIT_SCRIPT=%IDF_PATH%\export.bat"
IF NOT EXIST %IDF_INIT_SCRIPT% (
    :: MORE %SCRIPTDIR%readme.md
    ECHO Script %IDF_INIT_SCRIPT% does not exist!.
    GOTO :DIE
)
GOTO :RUNSCRIPT

:RUNSCRIPT
ECHO call %IDF_INIT_SCRIPT%
CALL %IDF_INIT_SCRIPT%
IF !ERRORLEVEL! NEQ 0 goto :DIE

:DONE
: Restore the Color of the screen
COLOR 07
ECHO/ 
ECHO   idf build flash monitor -p COM15
GOTO :SHELL

:DIE
: Color the screen red
COLOR 4f
ECHO ERROR!
PAUSE
GOTO :QUIT

:QUIT
: Restore the Color of the screen
COLOR 07
GOTO :eof

:SHELL
ECHO/ 
cmd /k
