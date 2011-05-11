@ECHO OFF
SETLOCAL

SET "SUBWCREV=SubWCRev.exe"

"%SUBWCREV%" . "include\Version_rev.h.in" "include\Version_rev.h" -f
IF %ERRORLEVEL% NEQ 0 GOTO NoSubWCRev

"%SUBWCREV%" . "src\apps\mplayerc\res\mpc-hc.exe.manifest.conf" "src\apps\mplayerc\res\mpc-hc.exe.manifest" -f >NUL
IF %ERRORLEVEL% NEQ 0 GOTO NoSubWCRev

ENDLOCAL
EXIT /B


:NoSubWCRev
ECHO. & ECHO SubWCRev, which is part of TortoiseSVN, wasn't found!
ECHO You should (re)install TortoiseSVN.
ECHO I'll use MPC_VERSION_REV=0 for now.

ECHO #define MPC_VERSION_REV 3103       >  "include\Version_rev.h"
ECHO #define ACTUAL_BUILD_YEAR 2011     >> "include\Version_rev.h"
ECHO #define ACTUAL_BUILD_MONTH 5       >> "include\Version_rev.h"
ECHO #define ACTUAL_BUILD_DAY 11        >> "include\Version_rev.h"
ECHO #define ACTUAL_BUILD_TIME 17:12:00 >> "include\Version_rev.h"
COPY /Y /V "src\apps\mplayerc\res\mpc-hc.exe.manifest.template" "src\apps\mplayerc\res\mpc-hc.exe.manifest" >NUL

ENDLOCAL
EXIT /B
