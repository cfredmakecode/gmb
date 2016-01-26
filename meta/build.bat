@echo off
IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
del /Q *.*

REM build main dll
cl /MT /Gm- /GR- /EHa- /Od /Oi /WX /W4 /wd4100 /wd4211 /wd4505 /DLL /nologo /Z7 /Fagmb.assembly /Fdgmb.pdb /Fmgmb.map /Fegmb.dll /LD ..\gmb\code\gmb.cpp
REM build platform exe
cl /MT /Gm- /GR- /EHa- /Od /Oi /WX /W4 /wd4100 /wd4211 /wd4505 /nologo /Z7 /DWIN32_GMB_INTERNAL=1 ..\gmb\code\win32_gmb.cpp user32.lib gdi32.lib Dsound.lib
popd
