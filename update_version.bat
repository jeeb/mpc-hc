@ECHO OFF
SETLOCAL
SubWCRev .\ include\Version_rev.h.in include\Version_rev.h -f
IF %ERRORLEVEL% NEQ 0 GOTO NoSubWCRev

SubWCRev .\ src\apps\mplayerc\res\mpc-hc.exe.manifest.conf src\apps\mplayerc\res\mpc-hc.exe.manifest -f >NUL
IF %ERRORLEVEL% NEQ 0 GOTO NoSubWCRev

EXIT /B

:NoSubWCRev
ECHO NoSubWCRev, will use MPC_VERSION_REV=0

ECHO:#define MPC_VERSION_REV 3103       >  include\Version_rev.h
ECHO:#define ACTUAL_BUILD_YEAR 2011     >> include\Version_rev.h
ECHO:#define ACTUAL_BUILD_MONTH 5       >> include\Version_rev.h
ECHO:#define ACTUAL_BUILD_DAY 11        >> include\Version_rev.h
ECHO:#define ACTUAL_BUILD_TIME 17:12:00 >> include\Version_rev.h

COPY /Y /V src\apps\mplayerc\res\mpc-hc.exe.manifest.template src\apps\mplayerc\res\mpc-hc.exe.manifest

EXIT /B
