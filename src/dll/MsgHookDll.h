// MsgHookDll.h : Include file for standard system include files,
// Defines the exported functions for the DLL application.

#pragma once

#ifndef MSGHOOK_DLL // to avoid multiple inclusion
#define MSGHOOK_DLL "MsgHook.dll"

#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <stdlib.h>
// #include <string> // Prevent Exceptions: Do NOT include <string> or <vector>, as the target process may not be built with MinGW's libstdc++.
// #include <vector> // Prevent Exceptions: Do NOT include <string> or <vector>, as the target process may not be built with MinGW's libstdc++.
#include <Psapi.h>
#include <stdio.h>
#include <tlhelp32.h> //CreateToolhelp32Snapshot
#include <sddl.h> // Required for Security Descriptor string functions

#pragma comment( lib, "psapi.lib" )
//#pragma comment( lib, "kernel32.lib" )

#define SHARED_FILEMAP_NAME TEXT("Local\\MsgHookFixedSharedMemory") // Local allows non-admin apps to access it
//#define SHARED_FILEMAP_NAME TEXT("Global\\MsgHookFixedSharedMemory") // run as administrator, Global prefix allows access from services and different user sessions

// Structure to hold hook event data to send via WM_COPYDATA
typedef struct
{
	HWND hWnd;
	int nCode;
    int msg;
	DWORD dwHookType;
	WPARAM wParam;
	LPARAM lParam;
	DWORD wParamLen;
    DWORD lParamLen;
    // No actual string arrays here! They will follow in memory packed.
}HEVENT;

typedef struct
{
    // Use 'unsigned __int64' to force 8-byte storage for handles, supporting both 32-bit and 64-bit processes
    unsigned __int64 g_hWnd;        // Storage for HWND
    unsigned __int64 g_hInstance;   // Storage for HINSTANCE
    unsigned __int64 g_CwpHook;     // Storage for HHOOK
    unsigned __int64 g_CwpHookProc; // Storage for HOOKPROC
}GLOBALDATA;

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL); // Function pointer type for IsWow64Process

// Struct to pass data to the callback function
struct WindowSearchData {
    DWORD targetPid;
    HWND foundHwnd;
};

BOOL InitSharedFileMapping(HMODULE hModule);
void CleanupSharedFileMapping();

//void ExtractResource(const WORD nID, LPCTSTR szFilename);

// Helper to log to DebugView (Download Sysinternals DebugView to see these logs)
void DebugLog(const TCHAR* format, ...)
{
    TCHAR buffer[1024];
    va_list args;
    va_start(args, format);
    _vstprintf_s(buffer, 1024, format, args);
    va_end(args);
    OutputDebugString(buffer);
}

// This is the core logic that makes the header versatile for both building and using the DLL.
// #define BUILDING_DLL // Define this when testing compilation of the DLL itself
#ifdef BUILDING_DLL

    // If we are building the DLL, we want to export our functions.
    #define API_DECL __declspec(dllexport)
    // Exported functions to be called by the main executable
    extern "C" {
        API_DECL BOOL InstallMsgHook(HWND hListenerWnd, DWORD dwThreadId);
        API_DECL void UninstallMsgHook();
        API_DECL BOOL SetMsgHookWithFileMap(HWND callerHWnd, DWORD threadId);
        API_DECL BOOL RemoveHookWithFileMap();
        API_DECL HHOOK GetCurrentHookHandle();
        API_DECL void SetGlobalDLLInstance(HANDLE dllInstance);
        API_DECL BOOL IsCurrentProcess64Bit();
        API_DECL BOOL IsProcess64Bit(DWORD procId);
        API_DECL DWORD GetProcessMainThreadId(DWORD procId);
        API_DECL HWND GetHwndFromPID(DWORD pid);
        API_DECL LRESULT CALLBACK CwpHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    }
    
    inline BOOL LoadDllFunctions(HINSTANCE hDll)
    {
        return TRUE; // No-op when building the DLL
    }

#else

    // If we are using the DLL, we want to import its functions.
    #define API_DECL __declspec(dllimport)
    
    // For Dynamic Running of this DLL we can define function pointer types of each method.
    typedef BOOL  (*InstallMsgHookFunc)(HWND hListenerWnd, DWORD dwThreadId);
    typedef void  (*UninstallMsgHookFunc)();
    typedef BOOL  (*SetMsgHookWithFileMapFunc)(HWND callerHWnd, DWORD threadId);
    typedef BOOL  (*RemoveHookWithFileMapFunc)();
    typedef HHOOK (*GetCurrentHookHandleFunc)();
    typedef DWORD (*GetProcessMainThreadIdFunc)(DWORD procId);
    typedef void  (*SetGlobalDLLInstanceFunc)(HANDLE dllInstance);
    typedef BOOL  (*IsCurrentProcess64BitFunc)();
    typedef BOOL  (*IsProcess64BitFunc)(DWORD procId);
    typedef HWND  (*GetHwndFromPIDFunc)(DWORD pid);


    InstallMsgHookFunc InstallMsgHook;
    UninstallMsgHookFunc UninstallMsgHook;
    SetMsgHookWithFileMapFunc SetMsgHookWithFileMap;
    RemoveHookWithFileMapFunc RemoveHookWithFileMap;
    GetCurrentHookHandleFunc GetCurrentHookHandle;
    GetProcessMainThreadIdFunc GetProcessMainThreadId;
    IsCurrentProcess64BitFunc IsCurrentProcess64Bit;
    IsProcess64BitFunc IsProcess64Bit;
    GetHwndFromPIDFunc GetHwndFromPID;

    // Inline functions that return static function pointers


    // For Dynamic Running of this DLL we can define function to load each method.
    // This function loads the DLL and retrieves the function pointers.
    // HINSTANCE hDll = LoadLibraryA("MsgHook.dll");
    inline BOOL LoadDllFunctions(HINSTANCE hDll)
    {
        if (!hDll) return FALSE;

        InstallMsgHook = (InstallMsgHookFunc)GetProcAddress(hDll, "InstallMsgHook");
        UninstallMsgHook = (UninstallMsgHookFunc)GetProcAddress(hDll, "UninstallMsgHook");
        SetMsgHookWithFileMap = (SetMsgHookWithFileMapFunc)GetProcAddress(hDll, "SetMsgHookWithFileMap");
        RemoveHookWithFileMap = (RemoveHookWithFileMapFunc)GetProcAddress(hDll, "RemoveHookWithFileMap");
        GetCurrentHookHandle = (GetCurrentHookHandleFunc)GetProcAddress(hDll, "GetCurrentHookHandle");
        GetProcessMainThreadId = (GetProcessMainThreadIdFunc)GetProcAddress(hDll, "GetProcessMainThreadId");
        IsCurrentProcess64Bit = (IsCurrentProcess64BitFunc)GetProcAddress(hDll, "IsCurrentProcess64Bit");
        IsProcess64Bit = (IsProcess64BitFunc)GetProcAddress(hDll, "IsProcess64Bit");
        GetHwndFromPID = (GetHwndFromPIDFunc)GetProcAddress(hDll, "GetHwndFromPID");

        // Ensure all function pointers were loaded successfully
        return (InstallMsgHook && UninstallMsgHook && SetMsgHookWithFileMap && RemoveHookWithFileMap && GetCurrentHookHandle 
                && IsCurrentProcess64Bit && IsProcess64Bit && GetProcessMainThreadId && GetHwndFromPID);
    }

#endif


#endif // MSGHOOK_DLL