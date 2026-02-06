/*
 * MsgHookWin.cpp : Creates a GUI application that can receive messages from the MsgHookDll and display them in a text box
 *
 * Created by ejakubowski7@gmail.com
*/

#include <windows.h>
#include <tchar.h>
#include <commctrl.h> // for tooltips
#pragma comment(lib, "comctl32.lib") // Ensure linker finds it
#include <iostream>
#include <string>
#include "../dll/MsgHookDll.h"
#include "resource.h"
#include "MsgLookup.h"


// Define Control IDs for the new Toolbar
#define IDC_BTN_FINDER     2001
#define IDC_EDIT_HWND_TB   2002
#define IDC_EDIT_PID_TB    2003
#define IDC_BTN_START_TB   2004
#define TOOLBAR_HEIGHT     40
#define MAX_LOADSTRING 100

// Global Variables:
#define TXTBOX_LIMIT 700000
#define MSGHOOKCLI32_EXE L"MsgHookCli32.exe"

HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HWND mainHwnd = NULL;
HMENU mainMenu = NULL;
HWND txtbox = NULL;

BOOL isHookStarted = FALSE;

// Toolbar Controls
HWND hBtnFinder = NULL;
HWND hEditHwnd = NULL;
HWND hEditPid = NULL;
HWND hBtnStartStopHook = NULL;
HWND hToolTip = NULL;
WNDPROC wpOldFinder = NULL; // Original window proc for the finder button
HCURSOR hCursorCross = NULL;
HCURSOR hCursorNormal = NULL;
BOOL isDragging = FALSE;

HWND targetHwnd = NULL;
DWORD targetPid = 0;
std::wstring targetExeName = L"";

const int txtboxSpacing = 2;

long msgCount = 0;

//message filters flags
bool filterWmCommand = false;
bool filterWmNotify = false;
bool filterCustom = false;
bool filterAbove = false;

#define MAX_TEST_SIZE 100
//TCHAR targetClassname[MAX_TEST_SIZE] = _T("Notepad");
TCHAR targetProcessId[MAX_TEST_SIZE] = _T("");
TCHAR targetClassname[MAX_TEST_SIZE] = _T("");
TCHAR targetHwndStr[MAX_TEST_SIZE] = _T("");
TCHAR testWmSettextL[MAX_TEST_SIZE] = _T("This is a test");
TCHAR testWmSettextW[MAX_TEST_SIZE] = _T("0");
TCHAR testWmCommandL[MAX_TEST_SIZE] = _T("0");
TCHAR testWmCommandW[MAX_TEST_SIZE] = _T("1");

TCHAR customMsgStr[MAX_TEST_SIZE] = _T("WM_SETTEXT");

const int hotkeyIdOffset = 0;
const int pauseHotKey = 'P'; //P
bool isPaused = false;

// Forward declarations of functions included in this code module:
int APIENTRY StartWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow);
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	DlgProc(HWND, UINT, WPARAM, LPARAM);
void StartMessageHook();

void AppendText(HWND txtHwnd, LPCTSTR newText)
{
	if (isPaused)
		return;
	DWORD len = GetWindowTextLength(txtHwnd);
	if (len > (TXTBOX_LIMIT - 500))
	{//need to truncate the beginning so the text doesn't go past it's limit
		SendMessage(txtHwnd, EM_SETSEL, 0, 20000);
		SendMessage(txtHwnd, EM_REPLACESEL, 0, (LPARAM)_T(""));
		len = GetWindowTextLength(txtHwnd);
	}
	//DWORD l,r;
	//SendMessage(txtHwnd, EM_GETSEL,(WPARAM)&l,(LPARAM)&r);
	SendMessage(txtHwnd, EM_SETSEL, len, len);
	SendMessage(txtHwnd, EM_REPLACESEL, 0, (LPARAM)newText);
	len = GetWindowTextLength(txtHwnd);
	SendMessage(txtHwnd, EM_SETSEL, len, len);
	//SendMessage(txtHwnd, EM_SETSEL,l,r);
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


// --- DRAG AND DROP FINDER LOGIC ---

// Helper to update UI fields based on a found window
void UpdateTargetInfo(HWND foundHwnd)
{
    if (foundHwnd)
    {
        // Update HWND Box
        TCHAR tmp[64];
        _stprintf_s(tmp, _T("0x%llX"), foundHwnd);
        SetWindowText(hEditHwnd, tmp);

        // Update PID Box
        DWORD pid = 0;
        GetWindowThreadProcessId(foundHwnd, &pid);
        _stprintf_s(tmp, _T("%u"), pid);
        SetWindowText(hEditPid, tmp);

        // Temporarily store in global strings (committed on mouse up)
        _stprintf_s(targetHwndStr, _T("%llu"), (unsigned __int64)foundHwnd);
        _stprintf_s(targetProcessId, _T("%u"), pid);
        targetPid = pid;
        targetHwnd = foundHwnd;
    }
}

// Subclassed Window Procedure for the Finder Button
LRESULT CALLBACK FinderBtnProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        {
            isDragging = TRUE;
            SetCapture(hWnd); // Capture mouse input even outside the window
            SetCursor(hCursorCross);
            SetWindowText(hWnd, _T("X")); // Visual cue
        }
        return 0;

    case WM_MOUSEMOVE:
        if (isDragging)
        {
            // Get global cursor position
            POINT pt;
            GetCursorPos(&pt);
            
            // Find window under cursor
            HWND foundHwnd = WindowFromPoint(pt);
            
            // Highlight logic could go here (DrawFocusRect), skipped for brevity
            
            // Update the Toolbar Text Fields
            if (foundHwnd != mainHwnd && foundHwnd != hWnd) // Don't pick self
            {
                UpdateTargetInfo(foundHwnd);
            }
        }
        break;

    case WM_LBUTTONUP:
        if (isDragging)
        {
            isDragging = FALSE;
            ReleaseCapture();
            SetCursor(hCursorNormal);
            SetWindowText(hWnd, _T("[+]")); // Reset icon text
            
            // Get final position
            POINT pt;
            GetCursorPos(&pt);
            HWND foundHwnd = WindowFromPoint(pt);
            if (foundHwnd != mainHwnd && foundHwnd != hWnd)
            {
                 UpdateTargetInfo(foundHwnd);
                 targetExeName = GetFilenameFromPid(targetPid);
                 TCHAR info[256];
                 _stprintf_s(info, _T("Target Locked: %s (PID: %d)\r\n"), targetExeName.c_str(), targetPid);
                 AppendText(txtbox, info);
            }
        }
        break;
    }
    return CallWindowProc(wpOldFinder, hWnd, uMsg, wParam, lParam);
}

// Helper to drill down from the Windows 10/11 "ApplicationFrameHost" wrapper to the actual UWP window (Windows.UI.Core.CoreWindow).
HWND GetRealUwpWindow(HWND hParent)
{
    TCHAR className[256];
    if (hParent != NULL && GetClassName(hParent, className, 256))
    {
        // Check if we grabbed the generic wrapper frame
        if (_tcscmp(className, _T("ApplicationFrameWindow")) == 0)
        {
            // Find the immediate child. UWP apps usually host their content in "Windows.UI.Core.CoreWindow"
            HWND hChild = FindWindowEx(hParent, NULL, _T("Windows.UI.Core.CoreWindow"), NULL);
            if (hChild) 
            {
                return hChild;
            }
        }
    }
    return hParent; // Return original if it wasn't a wrapper
}

// Helper to create the tooltip window and register a tool
void RegisterTooltip(HWND hParent, HWND hControl, LPCTSTR text)
{
    // Create the Tooltip Control (if it doesn't exist yet)
    if (!hToolTip)
    {
        hToolTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hParent, NULL, hInst, NULL);

        // Set the max width so it wraps nicely if text is long
        SendMessage(hToolTip, TTM_SETMAXTIPWIDTH, 0, 300);
    }

    // Register the "Tool" (The button) with the Tooltip
	TOOLINFO ti = { 0 };
    // Use the V1 size constant to ensure compatibility even if the app doesn't have a Visual Styles manifest.
    ti.cbSize = TTTOOLINFO_V1_SIZE; 
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = hParent;
    ti.uId = (UINT_PTR)hControl;
    ti.hinst = hInst;
    ti.lpszText = (LPTSTR)text;
    SendMessage(hToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

// Initialize Message Filters and Lookup Table
void InitMsgFiltersAndLookup()
{
	if (!filterWmCommand && !filterAbove && !filterWmNotify && !filterCustom)
		InitializeMsgLookup();
	else
	{
		int allowList[4];
		for (int i = 0; i < 4; i ++)
			allowList[i] = -1;
		
		if (filterWmCommand) {
			AppendText(txtbox, _T("filtering on WM_COMMAND & WM_MENUCOMMAND\r\n"));
			allowList[0] = WM_COMMAND;
			allowList[1] = WM_MENUCOMMAND;
		}
		if (filterWmNotify)
		{
			AppendText(txtbox, _T("filtering on WM_NOTIFY\r\n"));
			allowList[2] = WM_NOTIFY;
		}
		//if (filterAbove)
		//	allowList[0] = WM_COMMAND;
		if (filterCustom && _tcslen(customMsgStr) > 0) 
		{
			InitializeMsgLookup(); //initialize full msg list and do reverse lookup based on custom filter string
			for (int x = 0; x < MAX_MSG_LOOKUP; x++)
			{
				if (_tcscmp(customMsgStr, MSG_LOOKUP[x]) == 0) {
					TCHAR tmp[100];
					_stprintf_s(tmp, _T("filtering on %s (%d)\r\n"), customMsgStr, x);
					AppendText(txtbox, tmp);
					allowList[3] = x;
				}
			}
		}
		InitializeMsgLookup(allowList, 4);
	}
}

// Start the Message Hook by getting target info and calling the install hook function
void StartMessageHook()
{
	AppendText(txtbox, _T("Starting Message Hook\r\n"));
	//targetHwnd = FindWindow(targetClassname, NULL);
	
	TCHAR tmp[500];
	
	DWORD tid = 0;

	if (_tcscmp(targetHwndStr, _T("")) != 0) //if target HWND was used
	{
		TCHAR *stopStr;
		targetHwnd = (HWND)_tcstoull(targetHwndStr, &stopStr, 10);
		targetHwnd = GetRealUwpWindow(targetHwnd); // If it's a UWP wrapper, drill down to the real window
		tid = GetWindowThreadProcessId(targetHwnd, NULL);
		_stprintf_s(tmp, _T("Target Handle: %ld, and Thread Id: %ld\r\n"), targetHwnd, tid);
	}

	targetPid = 0;
	if (_tcscmp(targetProcessId, _T("")) != 0) //if target pid was used
	{
		TCHAR *stopStr;
		targetPid = (DWORD)_tcstoull(targetProcessId, &stopStr, 10);
		targetHwnd = GetHwndFromPID(targetPid);
		DWORD new_tid = GetWindowThreadProcessId(targetHwnd, NULL);
		if (new_tid != 0) tid = new_tid;
		targetExeName = GetFilenameFromPid(targetPid);
		_stprintf_s(tmp, _T("Application: %s, Target PId: %ld, and Thread Id: %ld\r\n"), targetExeName.c_str(), targetPid, tid);		
	}
	AppendText(txtbox, tmp);
	if (targetPid == 0 && targetHwnd != NULL) // if only target hwnd was used, try to fill in the pid and exe name for better info display and filtering
	{
		tid = GetWindowThreadProcessId(targetHwnd, &targetPid);
		targetExeName = GetFilenameFromPid(targetPid);
		_stprintf_s(tmp, _T("Application: %s, Target PId: %ld, and Thread Id: %ld\r\n"), targetExeName.c_str(), targetPid, tid);		
		return;
	}

	InitMsgFiltersAndLookup();
	//InitializeMsgLookup();
	
	//block self/global msg hook
	if (tid == 0) {
		AppendText(txtbox, _T("Target thread not found\r\n"));
		return;
	}
	
	if (targetPid != 0) // handle various types of bit matching
	{
		BOOL current64bit = IsCurrentProcess64Bit();
		BOOL target64bit = IsProcess64Bit(targetPid);
		std::wstring targetBitness = IsProcess64Bit(targetPid) ? L"64-bit" : L"32-bit";
		if (current64bit == target64bit)
		{
			_stprintf_s(tmp, _T("Target PId (%ld) is a matching %s process\r\n"), targetPid, targetBitness.c_str());
			AppendText(txtbox, tmp);
		}
		else
		{
			_stprintf_s(tmp, _T("Target PId (%ld) is a NOT matching %s process.\r\n"), targetPid, targetBitness.c_str());
			AppendText(txtbox, tmp);
			std::wstring cmdLine = std::wstring(MSGHOOKCLI32_EXE) + L" " + std::to_wstring(targetPid) + L" " + std::to_wstring((DWORD)(ULONG_PTR)mainHwnd);
			_stprintf_s(tmp, _T("Launching %s to hook the target process...\r\n"), cmdLine.c_str());
			AppendText(txtbox, tmp);
			// Launch the 32-bit helper if target is 32-bit and we're 64-bit (since 64-bit processes can't inject into 32-bit processes)
			STARTUPINFO si = { sizeof(si) };
			PROCESS_INFORMATION pi;
			if (CreateProcess(NULL, cmdLine.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				return; // Exit the 64-bit launcher since the 32-bit helper is now running
			}
			else
			{
				_stprintf_s(tmp, _T("Failed to launch %s. Error: %d\r\n"), MSGHOOKCLI32_EXE, GetLastError());
				AppendText(txtbox, tmp);
				return;
			}
		}
	}
	if (SetMsgHookWithFileMap(mainHwnd, tid))
	//if (InstallMsgHook(mainHwnd, tid))
	{
		isHookStarted = true;
		SetWindowText(hBtnStartStopHook, _T("Stop Hook"));
		EnableMenuItem(mainMenu, ID_FILE_STOPHOOK, MF_ENABLED);
		EnableMenuItem(mainMenu, ID_FILE_STARTHOOK, MF_DISABLED | MF_GRAYED);
		AppendText(txtbox, _T("Hook successfully initialized\r\n"));
	}
	else
		AppendText(txtbox, _T("Hook failed to initialize\r\n"));
}

void StopMessageHook()
{
	EnableMenuItem(mainMenu, ID_FILE_STOPHOOK, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(mainMenu, ID_FILE_STARTHOOK, MF_ENABLED);
	AppendText(txtbox, TEXT("Stopping Message Hook\r\n"));
	//KillHook();
	RemoveHookWithFileMap();
	//UninstallMsgHook();
	isHookStarted = false;
	SetWindowText(hBtnStartStopHook, _T("Start Hook"));
    MSG msg;
    while(PeekMessage(&msg, mainHwnd, WM_COPYDATA, WM_COPYDATA, PM_REMOVE))
    {
		// Allow the message queue to drain any pending WM_COPYDATA messages
		//    that were sent *before* we unhooked. This prevents them from accessing dead memory.
		//TranslateMessage(&msg);
		//DispatchMessage(&msg);
	}
	msgCount = 0;
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
        const wchar_t* pLParamStr = (wchar_t*)pRawData; // LParam string starts immediately after the HEVENT struct
        if (hevent.lParamLen == 0)
            pLParamStr = emptyBuffer;

        TCHAR msgName[MAX_MSG_NAME];
        GetMsgNameFromMsgId(hevent.msg, msgName, MAX_MSG_NAME);

        TCHAR buffer[256];
        _stprintf_s(buffer, _T("%s[%d]: HWND: %llu | Msg: %d (%s) | wParam: %llu | lParam: %llu (%s)\r\n"), 
            targetExeName.c_str(), targetPid, hevent.hWnd, hevent.msg, msgName, hevent.wParam, hevent.lParam, pLParamStr);
        
        //OutputDebugString(buffer); // View in Visual Studio Output
        //std::wcout << buffer;      // View in Console
		AppendText(txtbox, buffer); // View in Text Box
        return TRUE;
    }
    return FALSE;
}

void SendWmSettext() //ID_TESTMSGS_WM
{
	//SetWindowText(targetHwnd, _T("This is a test"));
	//TCHAR txt[] = _T("This is a test");
	TCHAR *stopStr;
	long wparam = _tcstoull(testWmSettextW, &stopStr, 10);
	SendMessage(targetHwnd, WM_SETTEXT, wparam, (LPARAM)testWmSettextL);
	//PostMessage(targetHwnd, WM_SETTEXT, 0 , (LPARAM)txt);
}


void SendWmCommand() //ID_TESTMSGS_WM
{
	TCHAR *stopStr;
	HWND sendHwnd = targetHwnd;
	if (_tcscmp(targetHwndStr, _T("")) != 0)
	{
		sendHwnd = (HWND)_tcstoull(targetHwndStr, &stopStr, 10);
	}
	long wparam = _tcstoull(testWmCommandW, &stopStr, 10);
	long lparam = _tcstoull(testWmCommandL, &stopStr, 10);
	SendMessage(sendHwnd, WM_COMMAND, wparam, lparam);

	/*
	TCHAR tmp[500];
	_stprintf_s(tmp, _T("hook handle %ld\r\n"), (long)GetCurrentHookHandle());
	AppendText(txtbox, tmp); */
}

void HotKeyPressed(WPARAM wParam)
{
	//AppendText(txtbox, _T("hotkey test"));
	if (wParam == (pauseHotKey + hotkeyIdOffset))
	{
		if (!isPaused) 
		{
			AppendText(txtbox, _T("Paused\r\n"));
			isPaused = true;
		}
		else
		{
			isPaused = false;
			AppendText(txtbox, _T("Unpaused\r\n"));
		}
	}
}

//extern "C" __declspec(dllexport) 
void CreateMsgHookWindowx(LPTSTR lpCmdLine)
{
	//StartWinMain(GetModuleHandle(NULL), NULL, lpCmdLine, SW_SHOW);
	//StartWinMain((HINSTANCE)pData->g_hInstance, NULL, lpCmdLine, SW_SHOW);
	
}

// The main entry point for the Windows application.
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	return StartWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

int APIENTRY StartWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{

	//Initialize common controls for tooltips
	INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES; // Includes Tooltips, TreeViews, ListViews, etc.
    InitCommonControlsEx(&icex);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MSGHOOKTEST, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Extract the DLL resource if it doesn't already exist
	//ExtractResourceRc("MSGHOOK_FILE", "DLL", MSGHOOK_DLL);
	HMODULE hMod = LoadLibrary(GetDllFilenameBitWise());
	BOOL dllStatus = LoadDllFunctions(hMod);
	if (!dllStatus)
	{
		MessageBoxA(NULL, "Failed to load hook functions from DLL.", "Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MSGHOOKTEST));

	if (lpCmdLine != NULL) //process command line args
	{
		if (_tcslen(lpCmdLine) > 0)
		{
			TCHAR *stopStr;
			targetPid = (DWORD)_tcstoull(lpCmdLine, &stopStr, 10);
			_stprintf_s(targetProcessId, _T("%ld"), (long)targetPid);
			StartMessageHook();
		}
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		//if (msg.message == WM_HOTKEY)
		//	HotKeyPressed(msg.wParam);
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	UnregisterHotKey(mainHwnd, pauseHotKey + hotkeyIdOffset);

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MSGHOOKICO));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE+1); // Use button face color for background
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MSGHOOKTEST);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 750, 400, NULL, NULL, hInstance, NULL);

   if (!hWnd) {
	    DWORD lastErr = GetLastError();
		printf("Error Creating Window %d\n", lastErr);
		_tprintf(_T("Window Class Name: %s, Instance: %p\n"), szWindowClass, hInstance);
		return FALSE;
   }
   mainHwnd = hWnd;

   mainMenu = GetMenu(mainHwnd);

   // Create standard cursors for Drag operation
   hCursorNormal = LoadCursor(NULL, IDC_ARROW);
   hCursorCross  = LoadCursor(NULL, IDC_CROSS);

   EnableMenuItem(mainMenu, ID_FILE_STOPHOOK, MF_DISABLED | MF_GRAYED);

   RegisterHotKey(mainHwnd, pauseHotKey + hotkeyIdOffset, MOD_NOREPEAT | MOD_SHIFT | MOD_CONTROL, pauseHotKey); // CTRL + SHIFT + P

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   //set always on top
   SetWindowPos(mainHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE| SWP_NOMOVE);

   return TRUE;
}

void CreateChildWins(HWND hWnd)
{
    // Allow applications to send WM_COPYDATA to us
    ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD); 

    HFONT hFont = CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, 
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Arial"));

    // --- TOOLBAR UI ---
    int x = 5; int y = 5; int spacing = 10;

    // Finder Button (Target Icon)
    hBtnFinder = CreateWindow(_T("BUTTON"), _T("[+]"), 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER,
        x, y, 30, 30, hWnd, (HMENU)IDC_BTN_FINDER, hInst, NULL);
	//Create tooltip for finder button
	RegisterTooltip(hWnd, hBtnFinder, _T("Drag this Icon over a target window to select it."));
    
    // Subclass the button to handle dragging
    wpOldFinder = (WNDPROC)SetWindowLongPtr(hBtnFinder, GWLP_WNDPROC, (LONG_PTR)FinderBtnProc);
    x += 30 + spacing;

    // HWND Label & Edit
    CreateWindow(_T("STATIC"), _T("HWND:"), WS_CHILD | WS_VISIBLE, x, y+6, 55, 20, hWnd, NULL, hInst, NULL);
    x += 55;
    hEditHwnd = CreateWindow(_T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 
        x, y+5, 90, 20, hWnd, (HMENU)IDC_EDIT_HWND_TB, hInst, NULL);
	//Create tooltip for HWND edit box
	RegisterTooltip(hWnd, hEditHwnd, _T("Displays the handle of the selected target window."));
    SendMessage(hEditHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += 90 + spacing;

    // PID Label & Edit
    CreateWindow(_T("STATIC"), _T("PID:"), WS_CHILD | WS_VISIBLE, x, y+6, 35, 20, hWnd, NULL, hInst, NULL);
    x += 35;
    hEditPid = CreateWindow(_T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 
        x, y+5, 70, 20, hWnd, (HMENU)IDC_EDIT_PID_TB, hInst, NULL);
	//Create tooltip for PID edit box
	RegisterTooltip(hWnd, hEditPid, _T("Displays the process ID of the selected target window."));
    SendMessage(hEditPid, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += 70 + spacing;

    // Start and stop Hook Button
    hBtnStartStopHook = CreateWindow(_T("BUTTON"), _T("Start Hook"), 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, 100, 30, hWnd, (HMENU)IDC_BTN_START_TB, hInst, NULL);
	//Create tooltip for start/stop button
	RegisterTooltip(hWnd, hBtnStartStopHook, _T("Start or Stop the Message Hooking into the target process."));
    
    // MAIN LOG TxtBOX
    txtbox = CreateWindow(TEXT("Edit"),TEXT(""), 
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL | ES_READONLY, 
        0, 0, 0, 0, hWnd, NULL, NULL, NULL); // Sized in WM_SIZE
    
    SendMessage(txtbox, EM_SETLIMITTEXT, (WPARAM)TXTBOX_LIMIT, 0);
    SendMessage(txtbox, WM_SETFONT, (WPARAM)hFont, TRUE);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == txtbox) // Check if the control asking for color is our Log txtbox
            {
                SetBkColor(hdc, RGB(255, 255, 255)); // Text Background = White
                SetTextColor(hdc, RGB(0, 0, 0));     // Text Color = Black
                return (INT_PTR)GetStockObject(WHITE_BRUSH); // Box Background = White
            }
        }        
        break;
    case WM_CREATE:
        CreateChildWins(hWnd);
		break;
	case WM_COPYDATA:
		return (OnCopyData((COPYDATASTRUCT *) lParam));
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
        case IDC_BTN_START_TB: // Toolbar Start Button
			if (isHookStarted) 
			{
				StopMessageHook();
			}
			else 
			{
				StartMessageHook();
			}
			break;
		case ID_FILE_STARTHOOK:
			StartMessageHook();
			break;
		case ID_FILE_STOPHOOK:
			StopMessageHook();
			break;
		case ID_TESTMSGS_WM:
			SendWmSettext();
			break;
		case ID_TESTMSGS_WMCOM:
			SendWmCommand();
			break;
		case ID_PROC64TEST:
			if (_tcscmp(targetProcessId, _T("")) != 0) //if target pid was used
			{
				TCHAR tmp[500];
				TCHAR *stopStr;
				targetPid = (DWORD)_tcstoull(targetProcessId, &stopStr, 10);
				BOOL current64bit = IsCurrentProcess64Bit();
				if (IsProcess64Bit(targetPid) && current64bit)
					_stprintf_s(tmp, _T("Target pid (%ld) is a matching 64 bit process\r\n"), targetPid);
				else if(!IsProcess64Bit(targetPid) && !current64bit)
					_stprintf_s(tmp, _T("Target pid (%ld) is a matching 32 bit process\r\n"), targetPid);
				else if (IsProcess64Bit(targetPid))
					_stprintf_s(tmp, _T("Target pid (%ld) is 64 bit process\r\n"), targetPid);
				else
					_stprintf_s(tmp, _T("Target pid (%ld) is 32 bit process\r\n"), targetPid);
				AppendText(txtbox, tmp);
				//ExtractResource(IDR_SETMH32, _T("SetMsgHook32.exe"));
				//_stprintf_s(tmp, _T(" %s %ld %d"), dll32bitName, (long)mainHwnd, targetPid);
				//RunResource(IDR_SETMH32, tmp);

				//MessageBox(0, , _T("64 bit Test"), 0);
			}
			break;
		case ID_FILE_SETTINGS:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, DlgProc);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, DlgProc);
			break;
		case ID_FILE_CLEAR:
			SetWindowText(txtbox, _T(""));
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_HOTKEY:
		HotKeyPressed(wParam);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_SIZE:
        { 
            // Resize logic: Keep Toolbar fixed at top, stretch Edit Box below it
            int nWidth = LOWORD(lParam);
            int nHeight = HIWORD(lParam);
            // Log Box starts below Toolbar Height
            int logY = TOOLBAR_HEIGHT + txtboxSpacing;
            int logH = nHeight - logY - txtboxSpacing;
            if (logH < 0) logH = 0;
            SetWindowPos(txtbox, HWND_NOTOPMOST, txtboxSpacing, logY, nWidth-(txtboxSpacing*2), logH, SWP_NOZORDER);
        }
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			//IDC_EDIT1
			//SendDlgItemMessage(hDlg, IDC_EDIT1, WM_SETTEXT, 0 , (LPARAM)targetClassname);
			SendDlgItemMessage(hDlg, IDC_TARGETPID, WM_SETTEXT, 0 , (LPARAM)targetProcessId);
			if (filterWmCommand)
				SendDlgItemMessage(hDlg, IDC_CHECK_CMD, BM_SETCHECK, BST_CHECKED, 0);
			if (filterWmNotify)
				SendDlgItemMessage(hDlg, IDC_CHECK_NOT, BM_SETCHECK, BST_CHECKED, 0);
			if (filterAbove)
				SendDlgItemMessage(hDlg, IDC_CHECK_ABO, BM_SETCHECK, BST_CHECKED, 0);
			if (filterCustom)
				SendDlgItemMessage(hDlg, IDC_CUSTOMCHK, BM_SETCHECK, BST_CHECKED, 0);
			SendDlgItemMessage(hDlg, IDC_WMCOMW, WM_SETTEXT, 0 , (LPARAM)testWmCommandW);
			SendDlgItemMessage(hDlg, IDC_WMCOML, WM_SETTEXT, 0 , (LPARAM)testWmCommandL);
			SendDlgItemMessage(hDlg, IDC_WMSETW, WM_SETTEXT, 0 , (LPARAM)testWmSettextW);
			SendDlgItemMessage(hDlg, IDC_WMSETL, WM_SETTEXT, 0 , (LPARAM)testWmSettextL);
			SendDlgItemMessage(hDlg, IDC_HWND, WM_SETTEXT, 0 , (LPARAM)targetHwndStr);
			SendDlgItemMessage(hDlg, IDC_CUSTOMMSG, WM_SETTEXT, 0 , (LPARAM)customMsgStr);			
		}
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) //only save on OK
		{
			//GetDlgItemText(hDlg, IDC_EDIT1, targetClassname, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_TARGETPID, targetProcessId, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_WMCOMW, testWmCommandW, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_WMCOML, testWmCommandL, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_WMSETW, testWmSettextW, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_WMSETL, testWmSettextL, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_HWND, targetHwndStr, MAX_TEST_SIZE);
			GetDlgItemText(hDlg, IDC_CUSTOMMSG, customMsgStr, MAX_TEST_SIZE);
			// check filter options
			filterWmCommand = (SendDlgItemMessage(hDlg, IDC_CHECK_CMD, BM_GETCHECK, 0, 0) == BST_CHECKED); // the hard way
			filterWmNotify = (IsDlgButtonChecked(hDlg, IDC_CHECK_NOT) == BST_CHECKED);// the easy way
			filterAbove = (IsDlgButtonChecked(hDlg, IDC_CHECK_ABO) == BST_CHECKED);
			filterCustom = (IsDlgButtonChecked(hDlg, IDC_CUSTOMCHK) == BST_CHECKED);

			InitMsgFiltersAndLookup();
		}
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}