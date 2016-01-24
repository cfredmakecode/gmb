@echo off
mkdir ..\..\build
pushd ..\..\build
cl /nologo /Zi ..\gmb\code\win32_gmb.cpp user32.lib gdi32.lib Dsound.lib
popd
