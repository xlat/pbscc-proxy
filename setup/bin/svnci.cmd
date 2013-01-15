@echo off

rem usage: svnci path_to_commit message


rem echo svn commit %1 %2

set target="%CD%\"
set msg=""

if [%1]==[] goto start

if exist %1 (
	set target=%1 
	shift
)

if not [%1]==[] (
	set msg=%1
	shift
)

:start
start TortoiseProc.exe /command:commit /path:%target% /logmsg:%msg% /notempfile /closeonend:0