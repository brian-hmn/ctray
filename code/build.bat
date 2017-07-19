@echo off
setlocal

::  We want the root directory of ctray, which is one directory up.
::  %~dp0 will get the directory of this file, build.bat
set CTRAY_ROOT_DIR=%~dp0..
::  By adding .. to the end of the path, we can now use this to get 
::  evaluated absolute path.
for /f "delims=" %%G in ("%CTRAY_ROOT_DIR%") do ( set "CTRAY_ROOT_DIR=%%~fG" )

set CTRAY_BUILD_DIR=%CTRAY_ROOT_DIR%\build
set CTRAY_CODE_DIR=%CTRAY_ROOT_DIR%\code

if not exist "%CTRAY_BUILD_DIR%" ( mkdir "%CTRAY_BUILD_DIR%" )

pushd "%CTRAY_BUILD_DIR%"
xcopy /D /Y "%CTRAY_CODE_DIR%\settings.ctray" "%CTRAY_BUILD_DIR%\"
rc /nologo "%CTRAY_CODE_DIR%\ctray.rc"
move "%CTRAY_CODE_DIR%\ctray.res" .\ctray.res
cl /nologo /FC /Zi "%CTRAY_CODE_DIR%\ctray.cpp" ctray.res user32.lib gdi32.lib shell32.lib
popd