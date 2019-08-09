@echo off

pushd ..\build
cl -O2 -FC -Zi ..\code\win32_handmade.cpp
popd