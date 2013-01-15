@echo off


rem get nsis home
regedit /ea tmp.reg HKEY_LOCAL_MACHINE\SOFTWARE\NSIS
for /F "delims=" %%i in ('findstr /C:"@=" tmp.reg') do echo set TTT%%i >tmp.cmd
call tmp.cmd

del tmp.reg >nul 2>nul
del tmp.cmd >nul 2>nul

set TTT@=%TTT@:\\=\%
set TTT@=%TTT@:~0,-2%\makensis.exe"

rem we gor nsis path in TTT@ variable


rem get pbscc.dll version
del /Q pbscc.ver >nul 2>nul
rundll32 bin\pbscc.dll,PbSccVersion
if not exist pbscc.ver call :error "Error getting pbscc.dll version information"

rem expected pbscc.ver file created
for /F %%i in (pbscc.ver) do set pbscc_%%i

del /Q pbsccsetup_%PBSCC_VERSION%.zip >nul 2>nul

rem %TTT@% SvnProxy.nsi
%TTT@% pbsccsetup.nsi
if errorlevel 1 call :error "Installation compile error."


PKZIPC -add -lev=9 -path=none pbsccsetup_%PBSCC_VERSION%.zip pbsccsetup.exe pbscc.ver

del /Q *.exe >nul 2>nul
del /Q pbscc.ver >nul 2>nul

exit 0





:error
echo %~1
exit 1
