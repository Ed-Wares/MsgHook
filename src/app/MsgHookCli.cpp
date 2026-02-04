#include <windows.h>
#include <tchar.h>
#include <iostream>

#include "../dll/MsgHookDll.h"
#include "MsgLookup.h"

#define APP_NAME _T("MsgHook")

DWORD targetPid = 0;
std::wstring targetExeName = L"";

// find Process ID from Window Handle
DWORD GetProcessIdFromWindow(HWND hWnd) 
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    return pid;
}

// Get the full path of the executable from its PID
std::wstring GetFilenameFromPid(DWORD pid)
{
    std::wstring filename = L"";
    // Use PROCESS_QUERY_LIMITED_INFORMATION (available Vista+)
    // This allows you to query names of system processes that usually block PROCESS_QUERY_INFORMATION.
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess)
    {
        // Buffer to hold the path
        TCHAR buffer[MAX_PATH];
        DWORD size = MAX_PATH;

        // Query the full path filename of the process
        if (QueryFullProcessImageName(hProcess, 0, buffer, &size))
        {
            filename = buffer;
            size_t pos = filename.find_last_of(L"\\/"); // Extract just the filename
            if (pos != std::wstring::npos) {
                filename = filename.substr(pos + 1);
            }
        }
        CloseHandle(hProcess);
    }
    else
    {
        // Optional: Handle error (e.g., Access Denied for protected system PIDs like 0 or 4)
        // DWORD err = GetLastError();
    }
    return filename;
}

BOOL OnCopyData(COPYDATASTRUCT* pCds) // WM_COPYDATA lParam will have this struct
{
    if (pCds->dwData == 1) // Match our DLL magic number
    {
        // We received a message from the target process!
        HEVENT hevent = *(HEVENT*)pCds->lpData;
        // Unpack the Data from COPYDATASTRUCT and HEVENT stuct
        // The wParam and LParam string data starts immediately after the HEVENT struct
        BYTE* pRawData = (BYTE*)pCds->lpData + sizeof(HEVENT);
        wchar_t emptyBuffer[] = L"";
        const wchar_t* pWParamStr = (wchar_t*)pRawData; // Point to wParam String
        // Point to lParam String (It starts after wParam string ends)
        const wchar_t* pLParamStr = (wchar_t*)(pRawData + hevent.wParamLen);
        if (hevent.wParamLen == 0)
            pWParamStr = emptyBuffer;
        if (hevent.lParamLen == 0)
            pLParamStr = emptyBuffer;

        TCHAR msgName[MAX_MSG_NAME];
        GetMsgNameFromMsgId(hevent.msg, msgName, MAX_MSG_NAME);

        TCHAR buffer[256];
        _stprintf_s(buffer, _T("%s[%d]: HWND: %p | Msg: %d (%s) | wParam: %llu | lParam: %llu (%s)\n"), 
            targetExeName.c_str(), targetPid, hevent.hWnd, hevent.msg, msgName, (unsigned __int64)hevent.wParam, (unsigned __int64)hevent.lParam, pLParamStr);
        
        //OutputDebugString(buffer); // View in Visual Studio Output
        std::wcout << buffer;      // View in Console
        return TRUE;
    }
    return FALSE;
}

// Window Procedure for our Listener Window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
            // Allow applications to send WM_COPYDATA to us
            ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD); 
            return 0;
        case WM_COPYDATA:
            //std::wcout << L"Received WM_COPYDATA message.\n";
            return (OnCopyData((COPYDATASTRUCT *) lParam));
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Main Entry Point
int main()
{
    // Setup a minimal Invisible Window to receive messages
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("MsgHookListener");
    RegisterClass(&wc);

    HWND hListener = CreateWindow(_T("MsgHookListener"), _T("Listener"), 
                                  0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    if (!hListener) 
    {
        std::cout << "Failed to create listener window.\n";
        return 1;
    }

    // Load the DLL
    // Extract the DLL resource if it doesn't already exist
	//ExtractResourceRc("MSGHOOK_FILE", "DLL", MSGHOOK_DLL);
	HMODULE hMod = LoadLibraryA(MSGHOOK_DLL);
	BOOL dllStatus = LoadDllFunctions(hMod);
	if (!dllStatus)
	{
		std::cout << "Failed to load hook functions from DLL.\n";
		return 1;
	}

    InitializeMsgLookup(); // Setup message name lookup table

    // User Input
    std::cout << "Open target application to install a message hook on. \n";
    std::cout << "Enter the target application's PID: ";
    
    targetPid = 0;
    std::cin >> targetPid;

    if (targetPid == 0) 
    {
        std::cout << "Invalid PID entered.\n";
        return 1;
    }

    // Find the main window of the target process
    HWND targetHwnd = GetHwndFromPID(targetPid);
    if (targetHwnd == NULL)
    {
        std::cout << "Could not find main window for PID " << targetPid << ".\n";
        return 1;
    }
    //DWORD dwThreadId = GetProcessMainThreadId(targetPid);
    DWORD dwThreadId = GetWindowThreadProcessId(targetHwnd, NULL);
    targetExeName = GetFilenameFromPid(targetPid);
    BOOL is64bit = IsProcess64Bit(targetPid);
    std::wstring appBitness = is64bit ? L"64-bit" : L"32-bit";
    std::cout << "Hooking Process ID: " << targetPid << "...\n";
    std::wcout << "Hooking (" << appBitness << L") Application: " << targetExeName << "\n";
    std::cout << "Hooking Thread ID: " << dwThreadId << ", HWND : " << targetHwnd << "\n";

    // Install Hook
    //if (SetMsgHookWithFileMap(hListener, dwThreadId)) 
    if (InstallMsgHook(hListener, dwThreadId)) 
    {
        std::cout << "Hook Installed!\n";
        std::cout << "Interact with target application to trigger messages.\n";
        std::cout << "Press Ctrl+C to Exit.\n";

        // Message Loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } else {
        std::cout << "Failed to install hook. Error: " << GetLastError() << "\n";
    }

    UninstallMsgHook();
    //RemoveHookWithFileMap();
    return 0;
}