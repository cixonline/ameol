@echo off
setlocal

if exist "%ProgramFiles%\HTML Help Workshop\hhc.exe" set _HHC="%ProgramFiles%\HTML Help Workshop\hhc.exe"
if exist "%ProgramFiles(x86)%\HTML Help Workshop\hhc.exe" set _HHC="%ProgramFiles(x86)%\HTML Help Workshop\hhc.exe"

%_HHC% ameol2.hhp

:Exit
endlocal
