@echo off

rem Force setup to always use release binaries!

set _CONFIG="Release"

rem Build script for Ameol setup.

setlocal
if "%_AM2DEVROOT%" == "" goto NoRoot

if exist "%programfiles%\inno setup 5" set PATH=%ProgramFiles%\Inno Setup 5;%PATH%
if exist "%programfiles(x86)%\inno setup 5" set PATH=%ProgramFiles(x86)%\Inno Setup 5;%PATH%

mkdir %_AM2DEVROOT%\setup\source\>nul
mkdir %_AM2DEVROOT%\setup\source\addons\>nul
mkdir %_AM2DEVROOT%\setup\source\DICT\>nul
mkdir %_AM2DEVROOT%\setup\source\Sounds\>nul
mkdir %_AM2DEVROOT%\setup\source\script\>nul

echo Updating local copy of input files.
xcopy /dyqi %_AM2DEVROOT%\Extras\am2fold.ico %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\A2Setup2.ico %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\addrbook.def %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\tips.def %_AM2DEVROOT%\setup\source\

xcopy /dyqi %_AM2DEVROOT%\Extras\LIBRARYS.TXT %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\changes.txt %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\moderate.chm %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\modera32.adn %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\getmm32.adn %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\getmm.hlp %_AM2DEVROOT%\setup\source\addons\
xcopy /dyqi %_AM2DEVROOT%\Extras\getmm32.txt %_AM2DEVROOT%\setup\source\addons\

xcopy /dyqi %_AM2DEVROOT%\Build\Amdb32.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Amctrl32.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Ameol32.exe %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Ameol232.exe %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Amob32.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Amuser32.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\Zmodem32.afp %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Ameol232\ameol232.exe.manifest %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\RegExp.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\RegExp9x.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Build\SciLexer.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\winsparkle.dll %_AM2DEVROOT%\setup\source\

copy /y     %_AM2DEVROOT%\Extras\decoder.dll %_AM2DEVROOT%\setup\source\addons\decoder.adn

xcopy /dyqi %_AM2DEVROOT%\Extras\ssce32.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\Ssceamm.clx %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\SSCEBRM.CLX %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\Ssceamc.tlx %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\SSCEBRC.TLX %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\ACRONYMS.DIC %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\Comptrm1.dic %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\meditrm.dic %_AM2DEVROOT%\setup\source\DICT\
xcopy /dyqi %_AM2DEVROOT%\Extras\DICT\places.dic %_AM2DEVROOT%\setup\source\DICT\

xcopy /dyqi %_AM2DEVROOT%\Extras\sounds %_AM2DEVROOT%\setup\source\sounds\

xcopy /dyqi %_AM2DEVROOT%\Extras\cats.def %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Help\ameol2.chm %_AM2DEVROOT%\setup\source

xcopy /dyqi %_AM2DEVROOT%\Tutorial\tutorial.scr %_AM2DEVROOT%\setup\source\

xcopy /dyqi %_AM2DEVROOT%\Extras\Script\*.* %_AM2DEVROOT%\setup\source\Script\

xcopy /dyqi %_AM2DEVROOT%\Extras\beta_report.ini %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\report.ini %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\cix.dat %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\nwzmod32.afp %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\acronyms.lst %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\msvcrt.dll %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\Extras\zmcfg32.adn %_AM2DEVROOT%\setup\source\addons\

xcopy /dyqi %_AM2DEVROOT%\Extras\ameol2.ini %_AM2DEVROOT%\setup\source\
xcopy /dyqi %_AM2DEVROOT%\setup\changes.html %_AM2DEVROOT%\setup\source\

if "%_AM2SIGNPWD%" == "" goto SkipSign1
echo Signing files...

set _CODEURL=http://timestamp.comodoca.com/authenticode
set SIGNTOOL="..\Tools\signtool.exe"
%SIGNTOOL% sign /f "%_PFXPATH%" /p %_AM2SIGNPWD% /t %_CODEURL% /v %_AM2DEVROOT%\setup\source\Ameol232.exe
:SkipSign1

echo Building setup program...
call iscc /q /cc Setup32.iss /o%_AM2DEVROOT%\drops

for /f "tokens=2,3" %%a in (%_AM2DEVROOT%\Shared\include\version.bld) do set %%a=%%b
set _VERSTRING=%PRODUCT_MAX_VER%.%PRODUCT_MIN_VER%.%PRODUCT_BUILD%

if "%_AM2SIGNPWD%" == "" goto SkipSign2
echo Signing installers...
%SIGNTOOL% sign /f "%_PFXPATH%" /p %_AM2SIGNPWD% /t %_CODEURL% /v %_AM2DEVROOT%\drops\a%_VERSTRING%.exe
:SkipSign2

echo Fixing ameol2.ini
copy /y %_AM2DEVROOT%\Extras\ameol2.ini %_AM2DEVROOT%\setup\source\ameol2.ini
echo Version=%_VERSTRING% >>%_AM2DEVROOT%\setup\source\ameol2.ini

echo Building Lite distribution...
set ZIP7="..\Tools\7z.exe"
set ZARCHIVE=%_AM2DEVROOT%\drops\z%_VERSTRING%.zip
if exist %ZARCHIVE% del %ZARCHIVE%
%ZIP7% a -r %ZARCHIVE% %_AM2DEVROOT%\setup\source\*
goto Exit

:NoRoot
echo Error: _AM2DEVROOT must be set to the root folder of the Ameol enlistment.

:Exit
endlocal
