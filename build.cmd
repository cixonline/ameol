@echo off
setlocal

if "%_AM2DEVROOT%" == "" goto NoRoot

set _CONFIG="Debug"
set _OPTION=build
set _DOSETUP=
if "%1" == "" goto DoBuild

:NextArg
if "%1" == "debug" set _CONFIG="Debug"&&goto ShiftArg
if "%1" == "release" set _CONFIG="Release"&&goto ShiftArg
if "%1" == "clean" set _OPTION=rebuild&&goto ShiftArg
if "%1" == "" goto DoBuild
echo Error: Unknown parameter %1
goto Exit

:ShiftArg
shift
goto NextArg

:DoBuild
set "%VSCMD_VER%" == "" goto MissingVS
pushd %_AM2DEVROOT%\tutorial
call build.cmd %*
cd %_AM2DEVROOT%\Help
call build.cmd %*
cd %_AM2DEVROOT%
msbuild ameol.sln /t:%_OPTION% /p:Configuration=%_CONFIG%
cd %_AM2DEVROOT%\setup
call build.cmd %*
goto Exit

:MissingVS
echo Error: Microsoft Visual Studio not found. 
echo        Please run from a Visual Studio Developer Console.
goto Exit

:NoRoot
echo Error: _AM2DEVROOT must be set to the root folder of the Ameol2 enlistment.

:Exit
endlocal
