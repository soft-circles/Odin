@echo off
setlocal EnableDelayedExpansion

pushd "%~dp0" || exit /b 1

if not exist "..\lib" mkdir "..\lib"

rem wide8.c holds the AVX2 solver path and must be compiled with /arch:AVX2,
rem the rest stays baseline SSE2 and picks the AVX2 path at runtime.
set "SRCS="
for %%f in (src\*.c) do (
    if /i not "%%~nxf"=="wide8.c" set "SRCS=!SRCS! %%f"
)

cl -nologo -MT -TC -O2 -c -std:c17 -I"include" !SRCS!
if errorlevel 1 goto fail
cl -nologo -MT -TC -O2 -c -std:c17 -arch:AVX2 -I"include" src\wide8.c
if errorlevel 1 goto fail

lib -nologo *.obj -out:..\lib\box3d.lib
if errorlevel 1 goto fail

del "*.obj"

cl -nologo -MT -TC -O2 -c -std:c17 -Dbox3d_EXPORTS -I"include" !SRCS!
if errorlevel 1 goto fail
cl -nologo -MT -TC -O2 -c -std:c17 -arch:AVX2 -Dbox3d_EXPORTS -I"include" src\wide8.c
if errorlevel 1 goto fail

link -nologo -DLL *.obj -out:..\lib\box3d.dll -implib:..\lib\box3ddll.lib
if errorlevel 1 goto fail

del "*.obj"
popd
exit /b 0

:fail
del "*.obj" 2>nul
popd
exit /b 1
