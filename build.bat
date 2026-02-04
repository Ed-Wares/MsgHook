@echo off
SET prj_name=MsgHook
SET "current_dir=%~dp0"
SET "build_dir=%current_dir%build\"
SET "distrib_dir=%current_dir%distrib\%prj_name%\"

REM This is optional bat file to rebuild both DLL and EXE
REM You can run it from command line or integrate into VSCode tasks

cd /d "%current_dir%"
FOR /F "tokens=*" %%i IN ('where g++.exe') DO pushd "%%~dpi..\.." && (call set "MSYS_ROOT=%%CD%%") && popd
echo MSYS_ROOT: %MSYS_ROOT%

echo removing old build
rmdir /s /q "%build_dir%"
rmdir /s /q "%distrib_dir%"

echo creating build directory "%build_dir%"
mkdir "%build_dir%"
mkdir "%distrib_dir%"
pushd "%build_dir%"

echo dependencies can be installed in MinGW with: pacman -S --needed git zip mingw-w64-ucrt-x86_64-toolchain base-devel

echo building MsgHook.dll
g++ -shared -o MsgHook.dll %current_dir%src\dll\MsgHookDll.cpp -luser32 -lpsapi -static-libgcc -static-libstdc++ -DBUILDING_DLL -DUNICODE -D_UNICODE

echo Setting section flags in dll to make .shared section read-write-shared
objcopy --set-section-flags .shared=alloc,load,data,share MsgHook.dll
echo Verifying section flags:
objdump -h MsgHook.dll | findstr SHARED

echo building MsgHook.exe
windres %current_dir%src\app\resource.rc -o resource.o
g++ -o MsgHook.exe %current_dir%src\app\MsgHookWindow.cpp resource.o -L. -static -lgdi32 -mwindows -municode -DUNICODE -D_UNICODE

g++ -o MsgHookCli.exe %current_dir%src\app\MsgHookCli.cpp -L. -municode -static -luser32 -DUNICODE -D_UNICODE 

echo Setting permissions for ALL APPLICATION PACKAGES on MsgHook.dll
icacls MsgHook.dll /grant "ALL APPLICATION PACKAGES":(RX)

echo build test 64bit application
g++.exe -o calculator64.exe %current_dir%src\test\BasicCalculator.cpp -mwindows -static -lcomctl32

echo build 32bit applications
SET "OLD_PATH=%PATH%"
SET "PATH=%MSYS_ROOT%\mingw32\bin;%PATH%"
echo building MsgHook32.dll
g++ -shared -o MsgHook32.dll %current_dir%src\dll\MsgHookDll.cpp -luser32 -lpsapi -static-libgcc -static-libstdc++ -DBUILDING_DLL -DUNICODE -D_UNICODE
echo building MsgHookCli32.exe
g++ -o MsgHookCli32.exe %current_dir%src\app\MsgHookCli.cpp -L. -municode -static -luser32 -DUNICODE -D_UNICODE 
echo build test 32bit application
g++.exe -o calculator32.exe %current_dir%src\test\BasicCalculator.cpp -mwindows -static -lcomctl32

echo Setting section flags in 32.dll to make .shared section read-write-shared
objcopy --set-section-flags .shared=alloc,load,data,share MsgHook32.dll
icacls MsgHook32.dll /grant "ALL APPLICATION PACKAGES":(RX)

REM restore orginal PATH, which had 64bit mingw first
SET "PATH=%OLD_PATH%"


REM -mwindows: Links as a GUI application (removes the black console window behind it).
REM -static: Bundles libraries into the .exe so it runs on other computers without needing MinGW DLLs.
REM -lcomctl32: Links the Common Controls library (standard for UI).

echo copying binaries to the distrib folder...
copy /Y *.exe "%distrib_dir%"
copy /Y "%current_dir%LICENSE" "%distrib_dir%"
popd

pushd "%distrib_dir%.." && "%MSYS_ROOT%\usr\bin\zip.exe" -r %prj_name%.zip %prj_name% && popd
echo Created distribution file at distrib\%prj_name%.zip

