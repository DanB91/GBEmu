@echo off

set build=build
set CPPFLAGS=-D_CRT_SECURE_NO_WARNINGS -Zi -I include  -Wno-pragma-pack -Wno-int-to-void-pointer-cast -Wno-unknown-warning-option -I..\src\3rdparty

if "%1" == "help" (
	echo "Builds GBEmu for Windows. When no arguments are specified, then a release build is built."
	echo "Usage $0 [help | profile | clean]"
	echo "	help -- Prints this help message."
	echo "	profile -- Builds GBEmu where the profiler enabled and is accessible in the GBEmu debugger."
    echo "	clean -- Cleans the build directory."
	exit /b 0
)
if "%1" == "profile" (
	set CPPFLAGS=%CPPFLAGS% -DCO_PROFILEa
)
if "%1" == "clean" (
	del %build%\* /q 
	echo "Removed %build%"
	exit /b 0
)

.\ctime_windows.exe -begin buildtime.ctf
set CC="C:\Program Files\LLVM\bin\clang-cl"
set resources=resources
set /p version=< ..\version.txt
@echo constexpr char GBEMU_VERSION[] = "%version%"; > ..\src\version.h 

if not exist %build% mkdir %build%
if not exist %build%\imgui.obj %CC% %CPPFLAGS% -c ..\src\3rdparty\imgui.cpp -o %build%\imgui.obj
if not exist %build%\SDL2.dll copy lib\SDL2.dll %build%
del %build%\res.rc.bak
copy %resources%\* %build%  && .\ssed -i".bak" "s/@GBEmuVersion@/%version%/g" %build%\res.rc && rc -fo  %build%\res.res %build%\res.rc && %CC% %CPPFLAGS% ..\src\sdl_main.cpp -TP -c -o%build%\sdl_main.obj && %CC% %build%\sdl_main.obj %build%\res.res %build%\imgui.obj -Zi lib\SDL2Main.lib Opengl32.lib Shell32.lib ComDlg32.lib lib\SDL2.lib -o%build%\gbemu.exe -MT -link -SUBSYSTEM:WINDOWS
if %errorlevel% NEQ 0 echo Error compiling!
.\ctime_windows.exe -end buildtime.ctf