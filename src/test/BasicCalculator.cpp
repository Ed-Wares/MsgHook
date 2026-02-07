#include <windows.h>
#include <tchar.h>
#include <stdio.h>

// Control IDs
#define ID_EDIT 100
#define ID_BTN_0 1000
#define ID_BTN_1 1001
#define ID_BTN_2 1002
#define ID_BTN_3 1003
#define ID_BTN_4 1004
#define ID_BTN_5 1005
#define ID_BTN_6 1006
#define ID_BTN_7 1007
#define ID_BTN_8 1008
#define ID_BTN_9 1009
#define ID_BTN_PLUS 1101
#define ID_BTN_MINUS 1102
#define ID_BTN_MUL 1103
#define ID_BTN_DIV 1104
#define ID_BTN_EQUALS 1105
#define ID_BTN_CLEAR 1106
#define ID_BTN_DOT 1107

// Global variables for calculator state
HWND hEdit;
WNDPROC OldEditProc; // To store the original Edit Control procedure
double g_StoredValue = 0.0;
double g_CurrentValue = 0.0;
int g_LastOp = 0; // 0=None, 1=Add, 2=Sub, 3=Mul, 4=Div
BOOL g_NewEntry = TRUE; // Flag to clear text on next number entry

#define APP_TITLE _T("Basic Calculator")
TCHAR g_AppTitle[40] = APP_TITLE;

// Helper to check if the process is 64-bit. On 32-bit systems, a pointer is 4 bytes
bool Is64BitProcess() {
    return (sizeof(void*) == 8);
}

// Set the application title based on whether it's 32-bit or 64-bit
void SetAppTitleBits() {
    if (Is64BitProcess()) {
        _stprintf_s(g_AppTitle, 40, _T("%s (64)"), APP_TITLE);
    }
    else {
        _stprintf_s(g_AppTitle, 40, _T("%s (32)"), APP_TITLE);
    }
}

// Helper to set text in the display
void SetDisplay(double value) {
    TCHAR buf[256];
    _stprintf_s(buf, 256, _T("%.6g"), value); // %.6g removes trailing zeros
    SetWindowText(hEdit, buf);
}

// Helper to get text from display
double GetDisplay() {
    TCHAR buf[256];
    GetWindowText(hEdit, buf, 256);
    return _tcstod(buf, NULL);
}

// Perform calculation based on stored state
void DoCalculation() {
    double current = GetDisplay();
    double result = g_StoredValue;

    switch (g_LastOp) {
    case 1: result += current; break;
    case 2: result -= current; break;
    case 3: result *= current; break;
    case 4: if (current != 0) result /= current; else result = 0; break;
    }

    SetDisplay(result);
    g_StoredValue = result; // Allow chaining
    g_NewEntry = TRUE;
    g_LastOp = 0;
}

// Helper to handle operator logic
void HandleOperator(int opCode) {
    g_StoredValue = GetDisplay();
    g_NewEntry = TRUE;
    g_LastOp = opCode;
}

// Subclassed Edit Control Procedure
// This intercepts keyboard input BEFORE the Edit control sees it.
// Subclassed Edit Control Procedure
LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    // Trapping Focus and Click events. These standard events cause Windows to show the caret. We let Windows do 
    // its work (setting focus state) and then immediately hide the caret.
    case WM_SETFOCUS:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE: // Sometimes dragging selects text and shows caret
    {
        LRESULT ret = CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
        HideCaret(hwnd); 
        return ret;
    }

    // Trapping Input
    case WM_CHAR:
        // Handle Numbers (0-9)
        if (wParam >= '0' && wParam <= '9') {
            if (g_NewEntry) {
                SetWindowText(hwnd, _T(""));
                g_NewEntry = FALSE;
            }
            // Pass to default handler so the number appears
            LRESULT ret = CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
            HideCaret(hwnd); // Hide caret again after typing
            return ret;
        }
        // Handle Operators
        else if (wParam == '+') { HandleOperator(1); return 0; }
        else if (wParam == '-') { HandleOperator(2); return 0; }
        else if (wParam == '*') { HandleOperator(3); return 0; }
        else if (wParam == '/') { HandleOperator(4); return 0; }
        
        // Handle Enter/Equals
        else if (wParam == VK_RETURN || wParam == '=') {
            DoCalculation();
            SendMessage(hwnd, EM_SETSEL, 0, -1); // Select all text result
            HideCaret(hwnd); 
            return 0; 
        }
        // Handle Backspace
        else if (wParam == VK_BACK) {
            LRESULT ret = CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
            HideCaret(hwnd);
            return ret;
        }
        // Handle Dot
        else if (wParam == '.') {
            TCHAR currentText[256];
            GetWindowText(hwnd, currentText, 256);
            if (_tcschr(currentText, '.') == NULL) {
                if (g_NewEntry) {
                    SetWindowText(hwnd, _T("0."));
                    g_NewEntry = FALSE;
                    SendMessage(hwnd, EM_SETSEL, 2, 2); 
                    HideCaret(hwnd);
                    return 0;
                }
                // Allow default dot
                LRESULT ret = CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
                HideCaret(hwnd);
                return ret;
            }
            return 0; // Ignore second dot
        }
        return 0; // Swallow other keys (letters)
    }

    return CallWindowProc(OldEditProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
    {
        // 1. Create Display (Edit Control)
        // REMOVED: ES_READONLY
        hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, _T("EDIT"), _T("0"),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_RIGHT,
            10, 10, 260, 30, hwnd, (HMENU)ID_EDIT, GetModuleHandle(NULL), NULL);

        // Subclass the Edit Control
        OldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);

        // Define Fonts
        HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_SWISS, _T("Arial"));
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 2. Create Buttons
        const TCHAR* btnLabels[] = {
            _T("7"), _T("8"), _T("9"), _T("/"),
            _T("4"), _T("5"), _T("6"), _T("*"),
            _T("1"), _T("2"), _T("3"), _T("-"),
            _T("0"), _T("."), _T("C"), _T("+"),
            _T("=")
        };
        int btnIDs[] = {
            ID_BTN_7, ID_BTN_8, ID_BTN_9, ID_BTN_DIV,
            ID_BTN_4, ID_BTN_5, ID_BTN_6, ID_BTN_MUL,
            ID_BTN_1, ID_BTN_2, ID_BTN_3, ID_BTN_MINUS,
            ID_BTN_0, ID_BTN_DOT, ID_BTN_CLEAR, ID_BTN_PLUS,
            ID_BTN_EQUALS
        };

        // Grid positions
        int x = 10, y = 50, w = 60, h = 60;
        for (int i = 0; i < 16; i++) {
            HWND hBtn = CreateWindow(_T("BUTTON"), btnLabels[i],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x, y, w, h, hwnd, (HMENU)(INT_PTR)btnIDs[i], GetModuleHandle(NULL), NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            x += 65;
            if ((i + 1) % 4 == 0) { x = 10; y += 65; }
        }

        // Equals button (Wide, at bottom)
        HWND hEq = CreateWindow(_T("BUTTON"), _T("="),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 310, 255, 50, hwnd, (HMENU)ID_BTN_EQUALS, GetModuleHandle(NULL), NULL);
        SendMessage(hEq, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Set Focus to Edit control immediately so user can type
        SetFocus(hEdit);
    }
    break;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        
        // --- Keep Focus on Edit box even after clicking buttons ---
        // We do this at the end of the logic usually, but here we can just ensure
        // focus returns to hEdit if a button was clicked.
        if (id >= 1000) SetFocus(hEdit);

        // --- Number Buttons ---
        if (id >= ID_BTN_0 && id <= ID_BTN_9) {
            // Simulate keyboard input for consistency
            SendMessage(hEdit, WM_CHAR, '0' + (id - ID_BTN_0), 0);
        }
        // --- Dot ---
        else if (id == ID_BTN_DOT) {
            SendMessage(hEdit, WM_CHAR, '.', 0);
        }
        // --- Clear ---
        else if (id == ID_BTN_CLEAR) {
            g_StoredValue = 0.0;
            g_LastOp = 0;
            SetDisplay(0.0);
            g_NewEntry = TRUE;
            SendMessage(hEdit, EM_SETSEL, 0, -1);
        }
        // --- Operators ---
        else if (id == ID_BTN_PLUS) HandleOperator(1);
        else if (id == ID_BTN_MINUS) HandleOperator(2);
        else if (id == ID_BTN_MUL) HandleOperator(3);
        else if (id == ID_BTN_DIV) HandleOperator(4);
        
        // --- Equals ---
        else if (id == ID_BTN_EQUALS) {
            DoCalculation();
            SendMessage(hEdit, EM_SETSEL, 0, -1);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const TCHAR CLASS_NAME[] = _T("BasicCalcClass");

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    SetAppTitleBits();
    HWND hwnd = CreateWindow(CLASS_NAME, g_AppTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 420,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}