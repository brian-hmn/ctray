@echo off
pushd ..\..\build
rc ..\handmade\timer\ctray.rc
move ..\handmade\timer\ctray.res .\ctray.res
cl -FC -Zi ..\handmade\timer\ctray.cpp ctray.res user32.lib gdi32.lib shell32.lib
popd
