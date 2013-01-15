@echo off

rem /*
rem  * Copyright 2010 Dmitry Y Lukyanov
rem  *
rem  * Licensed under the Apache License, Version 2.0 (the "License");
rem  * you may not use this file except in compliance with the License.
rem  * You may obtain a copy of the License at
rem  *
rem  *     http://www.apache.org/licenses/LICENSE-2.0
rem  *
rem  * Unless required by applicable law or agreed to in writing, software
rem  * distributed under the License is distributed on an "AS IS" BASIS,
rem  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
rem  * See the License for the specific language governing permissions and
rem  * limitations under the License.
rem  *
rem  */



if not exist %MSVC_HOME%\bin\nmake.exe call :error "Environment variable MSVC_HOME not defined."

set path=%MSVC_HOME%\bin;%path%
set include=%MSVC_HOME%\include
set lib=%MSVC_HOME%\lib

if "%1"=="build"    call :make all
if "%1"=="b"        call :make all
if "%1"==""         call :make clean all
if "%1"=="r"        call :make clean all
if "%1"=="rebuild"  call :make clean all
if "%1"=="c"        call :make clean
if "%1"=="clean"    call :make clean

rem copy pbscc.dll to installation directory
copy /Y pbscc.dll ..\setup\bin


:end
exit 0

:make
nmake %1 %2
if errorlevel 1 call :error "Build failed."
exit /B 0



:error
echo %~1
exit 1
