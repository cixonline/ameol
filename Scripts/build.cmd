@echo off
echo Building Ameol...
pushd %_AM2DEVROOT%\tutorial
call build.cmd %*
cd %_AM2DEVROOT%\Help
call build.cmd %*
cd %_AM2DEVROOT%
call build.cmd %*
cd %_AM2DEVROOT%\setup
call build.cmd %*
echo Finished!
popd
