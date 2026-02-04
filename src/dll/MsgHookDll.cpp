// MsgHookDll.cpp : Defines the entry point for the DLL application.
// 
// Allows Messages to be hooked from other processes by installing a system-wide hook
// Creates a memory mapped file to share data between multiple instances of the DLL 
// 
// last modified by ejakubowski7@gmail.com

#include "MsgHookDll.h"

// --- SHARED MEMORY SECTION ---
// This tells the linker: "Put these variables in a special shared memory block"
// The OS ensures these values are identical across ALL processes loading this DLL.
// COMPILER OPTION: Link with /SECTION:.shared,RWS
// #pragma data_seg(".shared")
//     HWND g_HostHwnd = NULL;      // The window to send messages TO
//     HHOOK g_hHook = NULL;        // The hook handle
// #pragma data_seg()
// #pragma comment(linker, "/SECTION:.shared,RWS")

// #ifdef __INTELLISENSE__     
//     #define __attribute__(x) // tells IntelliSense into ignoring __attribute__ when not in windows-gcc-x64
// #endif

// Need to run: objcopy --set-section-flags .shared=alloc,load,data,share MsgHook.dll
unsigned __int64 g_HostHwnd_Shared __attribute__((section(".shared"))) = 0;
unsigned __int64 g_hHook_Shared    __attribute__((section(".shared"))) = 0;
HWND g_HostHwnd = NULL;
HHOOK g_hHook = NULL;

HINSTANCE g_hInst = NULL;

HANDLE hMappedFile;
GLOBALDATA* pData;

// Main entry point for the DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			g_hInst = hModule;
			DisableThreadLibraryCalls(hModule);
			InitSharedFileMapping(hModule);
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			CleanupSharedFileMapping();
			break; // on detaching the DLL, always unmap the view before closing the file mapping handle

	}
	return TRUE;
}

// initializes the shared memory mapping
BOOL InitSharedFileMapping(HMODULE hModule)
{
	if (pData != NULL) return TRUE; // Already created

	// Create a "Permissive" Security Descriptor.  This allows mosted hooked applications to access this object.
    //   String format: "D:" (DACL) "P" (Protected) "(A;;GA;;;WD)" (Allow Generic All to World/Everyone)
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    
    // Create a descriptor that allows "Everyone" (World) full access
    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
            TEXT("D:(A;;GA;;;WD)(A;;GA;;;AC)"), // WD=Everyone, AC=AppContainer
            SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
    {
        DebugLog(TEXT("MsgHook: Failed to create Security Descriptor. Error: %d"), GetLastError());
        return FALSE;
    }

	if (hMappedFile == NULL) //We use hMappedFile global if available, or create new
	{
		DebugLog(_T("MsgHook: CreateFileMapping name: %s"), SHARED_FILEMAP_NAME);
		hMappedFile = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(GLOBALDATA), SHARED_FILEMAP_NAME);
	}

	// Free the sa memory allocated by ConvertStringSecurityDescriptorToSecurityDescriptor
    if (sa.lpSecurityDescriptor) {
        LocalFree(sa.lpSecurityDescriptor);
    }
	
	if (hMappedFile == NULL) 
	{
		DebugLog(_T("MsgHook: [CRITICAL] CreateFileMapping failed. Error: %d"), GetLastError());
		return FALSE; // Fail DLL load if we can't map memory
	}

	pData = (GLOBALDATA*)MapViewOfFile(hMappedFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);

	if (pData == NULL) {
		DebugLog(_T("MsgHook: [CRITICAL] MapViewOfFile failed. Error: %d"), GetLastError());
		CloseHandle(hMappedFile);
		hMappedFile = NULL;
		return FALSE;
	}

	// Initialize only if newly created (The Source App), but don't zero out if it exists (Zombie check)
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		DebugLog(_T("MsgHook: Creating NEW memory segment."));
		memset(pData, 0, sizeof(GLOBALDATA));
		pData->g_hInstance = (unsigned __int64)hModule; 
	} 
	else 
	{
		DebugLog(_T("MsgHook: Opened EXISTING memory segment."));
	}
	
	return TRUE;
}

// Cleans up the shared memory mapping
void CleanupSharedFileMapping()
{
	DebugLog(_T("MsgHook: Cleaning up shared memory mapping."));
	if (pData != NULL) 
	{
		UnmapViewOfFile(pData);
		pData = NULL;
	}
	if (hMappedFile != NULL) 
	{
		DebugLog(_T("MsgHook: Closing file mapping handle."));
		CloseHandle(hMappedFile);
		hMappedFile = NULL;
	}
}

// converts wParam and lParam to wide strings based on whether the message is Unicode or ANSI
wchar_t* ConvertParamToWide(const void* srcParam, BOOL isUnicode)
{
    if (srcParam == NULL || IsBadReadPtr(srcParam, 1)) return NULL;

    wchar_t* outStr = NULL;

    if (isUnicode)
    {
        // Source is ALREADY Wide: Calculate length and Copy
        const wchar_t* wSrc = (const wchar_t*)srcParam;
        size_t len = 0;
        
        // Safe string length check (prevent infinite loop on bad data)
        while (len < 4096 && wSrc[len] != 0) len++; 

        size_t bytes = (len + 1) * sizeof(wchar_t);
        outStr = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
        if (outStr) memcpy(outStr, wSrc, bytes);
    }
    else
    {
        // Source is ANSI: Convert to Wide
        const char* cSrc = (const char*)srcParam;
        int len = MultiByteToWideChar(CP_ACP, 0, cSrc, -1, NULL, 0);
        
        if (len > 0 && len < 4096)
        {
            size_t bytes = len * sizeof(wchar_t);
            outStr = (wchar_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
            if (outStr) MultiByteToWideChar(CP_ACP, 0, cSrc, -1, outStr, len);
        }
    }
    return outStr;
}

LRESULT CALLBACK CwpHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// DebugLog(TEXT("MsgHook: CwpHookProc\n"));

	// optionally support updating the host hwnd from shared memory
	if (pData != NULL && pData->g_hWnd != 0 && g_HostHwnd == NULL)
	{
		g_HostHwnd = (HWND)(pData->g_hWnd); // update the host hwnd from shared memory
		if(g_hHook == NULL && pData->g_CwpHook != 0) g_hHook = (HHOOK)(pData->g_CwpHook); // update the hook handle from shared memory
	}

	// Update local cache from shared memory if needed
	if (g_HostHwnd == NULL && g_HostHwnd_Shared != 0)
		g_HostHwnd = (HWND)g_HostHwnd_Shared;

	if (g_hHook == NULL && g_hHook_Shared != 0)
		g_hHook = (HHOOK)g_hHook_Shared;

    // Always call next hook if nCode < 0
    if (nCode < 0) return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    // Only process if we have a valid Host Window to send to
    if (nCode == HC_ACTION && g_HostHwnd != NULL && IsWindow(g_HostHwnd))
    {
        CWPSTRUCT* cwps = (CWPSTRUCT*)lParam;

        // Prevent Infinite Loops: Don't hook our own messages
        if (cwps->hwnd != g_HostHwnd)
        {
            // FILTER: Ignore noisy messages to prevent crashing
            switch (cwps->message)
            {
                case WM_MOUSEMOVE:
                case WM_PAINT:
                case WM_ERASEBKGND:
                case WM_NCHITTEST:
                case WM_SETCURSOR:
                case WM_CTLCOLOREDIT:
                    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
            }

			// use plain C string logic ---
            wchar_t* pLParamStr = NULL;
            wchar_t* pWParamStr = NULL; // Usually empty

            if (cwps->message == WM_SETTEXT && cwps->lParam != 0)
            {
                pLParamStr = ConvertParamToWide((void*)cwps->lParam, IsWindowUnicode(cwps->hwnd));
            }

            // Calculate Sizes
            DWORD lSize = (pLParamStr) ? (lstrlenW(pLParamStr) + 1) * sizeof(wchar_t) : 0;
            DWORD wSize = 0;
            DWORD headerSize = sizeof(HEVENT);
            DWORD totalSize = headerSize + lSize + wSize;

            // Allocate PACKED buffer using Windows Heap
            BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalSize);
            
            if (buffer)
            {
                // Fill Header
                HEVENT* hEvent = (HEVENT*)buffer;
                hEvent->hWnd = cwps->hwnd;
                hEvent->msg = cwps->message;
                hEvent->wParam = cwps->wParam;
                hEvent->lParam = cwps->lParam;
                hEvent->wParamLen = 0;
                hEvent->lParamLen = lSize;

                // Copy String Data
                if (pLParamStr) {
                    memcpy(buffer + headerSize, pLParamStr, lSize);
                }

                // Send cds to Host Window
                COPYDATASTRUCT cds;
                cds.dwData = 1;
                cds.cbData = totalSize;
                cds.lpData = buffer;

                DWORD_PTR dwResult;
                SendMessageTimeout(g_HostHwnd, WM_COPYDATA, (WPARAM)cwps->hwnd, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NORMAL, 100, &dwResult);

                HeapFree(GetProcessHeap(), 0, buffer); // Cleanup Buffer
            }

            // Cleanup Strings
            if (pLParamStr) HeapFree(GetProcessHeap(), 0, pLParamStr);
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// This is the core logic that makes the header versatile for both building and using the DLL.
#ifdef BUILDING_DLL

// Installs a hook procedure that monitors messages before the system sends them to the destination window procedure. For more information, see the CallWndProc hook procedure.
BOOL InstallMsgHook(HWND callerHWnd, DWORD dwThreadId)
{
	DebugLog(TEXT("MsgHook: InstallMsgHook() %d\n"), dwThreadId);
    if (g_hHook != NULL) return TRUE; // Already hooked

	g_HostHwnd_Shared = (unsigned __int64)callerHWnd; // Store as 64-bit integer
    g_HostHwnd = callerHWnd; // Store shared HWND
    
    // Explicitly use the DLL Instance
    g_hHook = SetWindowsHookEx(WH_CALLWNDPROC, CwpHookProc, g_hInst, dwThreadId);
	g_hHook_Shared = (unsigned __int64)g_hHook; // Store as 64-bit integer
	DebugLog(TEXT("MsgHook: InstallMsgHook() Result: %d\n"), g_hHook != NULL);
    return (g_hHook != NULL);
}

// Exported Function to Stop the Hook
void UninstallMsgHook()
{
    if (g_hHook != NULL)
    {
		DebugLog(TEXT("MsgHook: UninstallMsgHook() \n"));
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
		g_hHook_Shared = 0;
		if (pData != NULL) 
		{
			pData->g_hWnd = 0;
			pData->g_CwpHook = 0;
			pData->g_CwpHookProc = 0;
		}
    }
    g_HostHwnd = NULL;
	g_HostHwnd_Shared = 0;
}

// Called from external program to setup the shared memory and install the hook
BOOL SetMsgHookWithFileMap(HWND callerHWnd, DWORD threadId)
{
	if (pData == NULL) 
	{
        DebugLog(_T("MsgHook: pData was NULL in SetMsgHook. Attempting lazy init..."));
		// Try to get the handle of the loaded DLL by name. Make sure this matches your actual DLL filename.
		HINSTANCE hInst = GetModuleHandle(TEXT(MSGHOOK_DLL));
		if (hInst == NULL ) hInst = GetModuleHandle(NULL); // fallback to current module
		if (!InitSharedFileMapping(hInst)) 
		{
			MessageBox(0, _T("Error: Shared Memory pData is NULL. DllMain failed?"), _T("Critical Error"), MB_ICONERROR);
			return FALSE;
		}
    }
	pData->g_hWnd = (unsigned __int64)callerHWnd; // remember the windows and hook handle for further instances
	if (pData->g_CwpHookProc == 0) pData->g_CwpHookProc = (unsigned __int64)CwpHookProc;
	
	// if the threadId = 0 means Global Hook (All processes)
	pData->g_CwpHook  = (unsigned __int64)SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)pData->g_CwpHookProc, (HINSTANCE)pData->g_hInstance, threadId);
	//pData->g_MsgHook  = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)MsgHookProc, (HINSTANCE)pData->g_hInstance, threadId);   
	if (pData->g_CwpHook == 0) {
		TCHAR tmp[100];
		_stprintf_s(tmp, _T("Last Error # %ld on threadId %ld"), GetLastError(), threadId);
		MessageBox(0, tmp, _T("Set Msg Hook Error"), 0);
	}
	return (pData->g_CwpHook != 0);
}

BOOL RemoveHookWithFileMap()
{
	if (pData == NULL)
	{
		DebugLog(_T("MsgHook: pData was NULL in RemoveHookWithFileMap. Nothing to do."));
		return false;
	}
	if(pData->g_CwpHook)       // if the hook is defined
	{
		DebugLog(_T("MsgHook: Removing Hook with FileMap"));
		BOOL ret = UnhookWindowsHookEx((HHOOK)pData->g_CwpHook);
		pData->g_hWnd = 0;  // reset data
		pData->g_CwpHook = 0;
		pData->g_CwpHookProc = 0;
        g_hHook = NULL;
		g_hHook_Shared = 0;
    	g_HostHwnd = NULL;
		g_HostHwnd_Shared = 0;
		return ret;
	}
	return false;
}

HHOOK GetCurrentHookHandle()
{
	return (HHOOK)pData->g_CwpHook; //if NULL hook isn't running
}

//testing if process 64 bit, needed to verify this dll can hook & attach to target process
BOOL IsCurrentProcess64Bit()
{
	return IsProcess64Bit(_getpid());
}

BOOL IsProcess64Bit(DWORD procId)
{
	SYSTEM_INFO stInfo;
	GetNativeSystemInfo(&stInfo); // if native system is x86 skip wow64 test
	if (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		return false; //printf( "Processor Architecture: Intel x86\n");

    BOOL bIsWow64 = FALSE;
    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.

    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
    if(fnIsWow64Process != NULL)
    {
		HANDLE procHandle = NULL;//GetCurrentProcess();
		procHandle = OpenProcess(PROCESS_QUERY_INFORMATION, false, procId);
        if (!fnIsWow64Process(procHandle, &bIsWow64))
        {
            //handle error
        }
		CloseHandle(procHandle);
		if (bIsWow64) // NOT a native 64bit process
			return false;
		return true;// is a native 64bit process
    }
    return false; //some error finding function "IsWow64Process" assume not 64-bit
}

DWORD GetProcessMainThreadId(DWORD procId)
{

#ifndef MAKEULONGLONG
	#define MAKEULONGLONG(ldw, hdw) ((ULONGLONG(hdw) << 32) | ((ldw) & 0xFFFFFFFF))
#endif
#ifndef MAXULONGLONG
	#define MAXULONGLONG ((ULONGLONG)~((ULONGLONG)0))
#endif

	DWORD dwMainThreadID = 0;
	ULONGLONG ullMinCreateTime = MAXULONGLONG;
	//includes all threads in the system
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap != INVALID_HANDLE_VALUE) {
		THREADENTRY32 th32;
		th32.dwSize = sizeof(THREADENTRY32);
		BOOL bOK = TRUE;
		//Enumerate all threads in the system and filter on th32OwnerProcessID = pid
		for (bOK = Thread32First(hThreadSnap, &th32); bOK ; bOK = Thread32Next(hThreadSnap, &th32)) {
			//if (th32.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(th32.th32OwnerProcessID)) {
			if (th32.th32OwnerProcessID == procId && (th32.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(th32.th32OwnerProcessID))) {
				//_tprintf(_T("DEBUG Enumerate Process (%ld) Thread Id: %ld\n"), procId, th32.th32ThreadID);
				HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, TRUE, th32.th32ThreadID);
				if (hThread) {
					FILETIME afTimes[4] = {0};
					if (GetThreadTimes(hThread,	&afTimes[0], &afTimes[1], &afTimes[2], &afTimes[3])) {
						ULONGLONG ullTest = MAKEULONGLONG(afTimes[0].dwLowDateTime, afTimes[0].dwHighDateTime);
						if (ullTest && ullTest < ullMinCreateTime) { //check each thread's creation time
							ullMinCreateTime = ullTest;
							dwMainThreadID = th32.th32ThreadID; // let it be main thread
						}
					}
					CloseHandle(hThread); //must close opened thread
				}
			}
		}
#ifndef UNDER_CE
		CloseHandle(hThreadSnap); //close thread snapshot
#else
		CloseToolhelp32Snapshot(hThreadSnap); //close thread snapshot
#endif
	}
	return dwMainThreadID; //returns main thread id or returns 0 if can't find it
}

// Callback function called for every window on the desktop
BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    WindowSearchData* pData = (WindowSearchData*)lParam;

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);

    // Check if this window belongs to our target process
    if (windowPid == pData->targetPid) {
        // OPTIONAL: Filter out hidden windows or child windows
        // Without this, you might grab a hidden "Default IME" window instead of the main UI.
        if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == NULL) {
            pData->foundHwnd = hwnd;
            return FALSE; // Stop enumerating
        }
    }
    return TRUE; // Continue enumerating
}

// Find the main window handle for a given PID
HWND GetHwndFromPID(DWORD pid) {
    WindowSearchData searchData;
    searchData.targetPid = pid;
    searchData.foundHwnd = NULL;
    // Start the search
    EnumWindows(EnumWindowsCallback, (LPARAM)&searchData);
    return searchData.foundHwnd;
}

#endif // BUILDING_DLL