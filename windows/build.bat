@echo off
%0\..\ctime_windows.exe -begin buildtime.ctf
set CC=clang-cl
set build=%0\..\build
set CPPFLAGS=-D_CRT_SECURE_NO_WARNINGS -Zi -I%0\..\include  -Wno-pragma-pack -Wno-int-to-void-pointer-cast -Wno-unknown-warning-option -I%0\..\..\src\3rdparty
if "%1" == "profile" (set CPPFLAGS=%CPPFLAGS% -DCO_PROFILE)
if not exist %build% mkdir %build%
if not exist %build%\imgui.obj %CC% %CPPFLAGS% -c ..\src\3rdparty\imgui.cpp -o%build%\imgui.obj
if not exist %build%\SDL2.dll copy %0\..\lib\SDL2.dll %build%

%CC% %CPPFLAGS% %0\..\..\src\sdl_main.cpp -TP -c -o%build%\sdl_main.obj && %CC% %build%\sdl_main.obj  %build%\imgui.obj -Zi %0\..\lib\SDL2Main.lib Opengl32.lib Shell32.lib ComDlg32.lib %0\..\lib\SDL2.lib -o%build%\gbemu.exe -MT -link -SUBSYSTEM:WINDOWS
if %errorlevel% NEQ 0 echo Error compiling!
%0\..\ctime_windows.exe -end buildtime.ctf
