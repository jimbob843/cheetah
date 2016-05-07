@echo off
set CHEETAHROOT=C:\Projects\Cheetah
set PATH=%PATH%;"C:\Program Files\Microsoft Visual Studio 8\VC\bin"
set PATH=%PATH%;%CHEETAHROOT%\Tools\Custom\Bin
set PATH=%PATH%;%CHEETAHROOT%\Tools\NASM
set PATH=%PATH%;%CHEETAHROOT%\Tools\DJGPP\bin
set PATH=%PATH%;%CHEETAHROOT%\Tools\UnixUtils\usr\local\wbin
set PATH=%PATH%;%CHEETAHROOT%\Tools\BFI
set DJGPP=%CHEETAHROOT%\Tools\DJGPP\djgpp.env

echo Cheetah build environment configured.

