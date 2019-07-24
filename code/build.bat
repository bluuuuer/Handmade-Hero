@echo off

pushd ..\build
cl -FC -Zi ..\code\win32_handmade.cpp
popd