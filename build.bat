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
g++ -o MsgHook.exe %current_dir%src\app\MsgHookWindow.cpp resource.o -L. -lMsgHook -lgdi32 -mwindows -municode -DUNICODE -D_UNICODE

g++ -o MsgHookCli.exe %current_dir%src\app\MsgHookCli.cpp -L. -lMsgHook -DUNICODE -D_UNICODE

echo Setting permissions for ALL APPLICATION PACKAGES on MsgHook.dll
icacls MsgHook.dll /grant "ALL APPLICATION PACKAGES":(RX)


echo copying binaries to the distrib folder...
copy /Y *.exe "%distrib_dir%"
copy /Y "%current_dir%LICENSE" "%distrib_dir%"
popd

pushd "%distrib_dir%.." && "%MSYS_ROOT%\usr\bin\zip.exe" -r %prj_name%.zip %prj_name% && popd
echo Created distribution file at distrib\%prj_name%.zip

