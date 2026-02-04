# MsgHook
The MsgHook application and library can display the internal window messages on any running windows process.

Internally the program create a windows message hooks to retreive various internal windows messages being processed by a windows application process.  This is done using the MsgHook.dll, which is built as a resource within the executable and is extracted at runtime.

This was originally designed for automation purposes in the Synthuse java application.

### Demo

Here is a quick look at the application:

![Demo](https://github.com/Ed-Wares/MsgHook/blob/main/DemoMsgHook.gif?raw=true)

## Usage
To start run MsgHook.exe



## Building

Build your own application binaries.

Prerequesites required for building source
-  msys2 - download the latest installer from the [MSYS2](https://github.com/msys2/msys2-installer/releases/download/2024-12-08/msys2-x86_64-20241208.exe)
- Run the installer and follow the steps of the installation wizard. Note that MSYS2 requires 64 bit Windows 8.1 or newer.
- Run Msys2 terminal and from this terminal, install the MinGW-w64 toolchain by running the following command:
```
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```
- Accept the default number of packages in the toolchain group by pressing Enter (default=all).
- Enter Y when prompted whether to proceed with the installation.
- Add the path of your MinGW-w64 bin folder (C:\msys64\ucrt64\bin) to the Windows PATH environment variable.
- To check that your MinGW-w64 tools are correctly installed and available, open a new Command Prompt and type:
```
gcc --version
g++ --version
gdb --version
```
- Optionally, install Visual Studio Code IDE (with C++ extensions).  [VsCode](https://code.visualstudio.com/download)

Build binaries by running the build.bat script or from VsCode by running the Build and Debug Task.

