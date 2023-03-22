@echo off

where cl >NUL 2>NUL
if %ERRORLEVEL% equ 0 goto :Compile

:InitVS2019
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >NUL 2>NUL

if %ERRORLEVEL% equ 0 goto :Compile

:InitVS2022
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >NUL 2>NUL

if %ERRORLEVEL% equ 0 goto :Compile

:NoCompiler
echo Visual Studio not installed
echo Supported versions are 2019 and 2022
goto :End

:Compile
if not exist "build" mkdir build
cd build
cl -Zi -GS- -Gs999999 -nologo ..\src\entry.c -link -nodefaultlib -subsystem:console kernel32.lib user32.lib shell32.lib -stack:0x1000000,0x1000000 /entry:WeDontNeedMain /OUT:slim64.exe
slim64.exe %1 %2
cd ..

:End
