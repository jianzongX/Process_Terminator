#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <shellapi.h>
#include <shlobj.h>

// ���尴ťID
#define ID_BUTTON1 101
#define ID_BUTTON2 102
#define ID_GOTO_BUTTON 103
#define ID_CLOSE_BUTTON 104

// ȫ�ֱ���
HWND g_hStatus = NULL;
HWND g_hMainWnd = NULL;
HWND g_hGotoButton = NULL;
const int STATUS_HEIGHT = 20;
const int GOTO_BUTTON_WIDTH = 150;
const int GOTO_BUTTON_HEIGHT = 50;

// ��������
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GotoWndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateStatus(HWND hwnd, const wchar_t* text);
void ShowMainWindow();
void CreateGotoButton();
bool IsUserAnAdmin();

// ������ԱȨ��
bool IsUserAnAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, 
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        if (!CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(AdministratorsGroup);
    }
    
    return isAdmin == TRUE;
}

// ʹ��taskkill������ֹ����
bool KillProcessWithTaskkill(const wchar_t* processName) {
    std::wstring command = L"taskkill /f /im \"" + std::wstring(processName) + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000); // �ȴ�5��
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
}

// �������Ƿ�����
bool IsProcessRunning(const wchar_t* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    bool result = false;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                result = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return result;
}

// ��ֹĿ�����
void KillTargetProcesses() {
    const std::vector<std::wstring> targets = {
        L"REDAgent.exe",
        L"checkrs.exe",
        L"rscheck.exe"
    };

    for (const auto& proc : targets) {
        // ʹ��taskkill������ֹ����
        if (!KillProcessWithTaskkill(proc.c_str())) {
            // ���taskkillʧ�ܣ�����ԭʼ����
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe;
                pe.dwSize = sizeof(PROCESSENTRY32W);
                
                if (Process32FirstW(hSnapshot, &pe)) {
                    do {
                        if (_wcsicmp(pe.szExeFile, proc.c_str()) == 0) {
                            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                            if (hProcess) {
                                TerminateProcess(hProcess, 0);
                                CloseHandle(hProcess);
                            }
                        }
                    } while (Process32NextW(hSnapshot, &pe));
                }
                CloseHandle(hSnapshot);
            }
        }
        
        // �����ӳ���ϵͳ����
        Sleep(200);
    }
}

// ˢ��ϵͳ����
void RefreshSystemTray() {
    HWND hTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (hTrayWnd) {
        HWND hNotifyWnd = FindWindowExW(hTrayWnd, NULL, L"TrayNotifyWnd", NULL);
        if (hNotifyWnd) {
            InvalidateRect(hNotifyWnd, NULL, TRUE);
            UpdateWindow(hNotifyWnd);
        }
    }
}

// ��������
void PerformCleanOperation(HWND hwnd) {
    UpdateStatus(hwnd, L"�����������...");
    KillTargetProcesses();
    
    // ������״̬
    bool redAgentRunning = IsProcessRunning(L"REDAgent.exe");
    bool checkrsRunning = IsProcessRunning(L"checkrs.exe");
    bool rscheckRunning = IsProcessRunning(L"rscheck.exe");
    
    if (!redAgentRunning && !checkrsRunning && !rscheckRunning) {
        UpdateStatus(hwnd, L"�������");
        MessageBoxW(NULL, L"���꣬������һƬ��������ĺ�\n          ף��������!", L"�����ɹ�", MB_ICONINFORMATION);
    } else {
        UpdateStatus(hwnd, L"ʧ��!\n����취:����");
        
        std::wstring msg = L"���½�����ֹʧ��:\n";
        if (redAgentRunning) msg += L"- REDAgent.exe\n";
        if (checkrsRunning) msg += L"- checkrs.exe\n";
        if (rscheckRunning) msg += L"- rscheck.exe\n";
        
        MessageBoxW(NULL, msg.c_str(), L"��������", MB_ICONWARNING);
    }
    RefreshSystemTray();
}

void PerformRestoreOperation(HWND hwnd) {
    UpdateStatus(hwnd, L"���ڻָ�����...");
    const std::wstring exePath = L"C:\\Program Files (x86)\\3000soft\\Red Spider\\REDAgent.exe";
    
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = exePath.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        UpdateStatus(hwnd, L"�ָ����");
        MessageBoxW(NULL, L"�����ѳɹ�����", L"�����ɹ�", MB_ICONINFORMATION);
    } else {
        UpdateStatus(hwnd, L"�ָ�ʧ��");
        MessageBoxW(NULL, L"��������ʧ��", L"��������", MB_ICONERROR);
    }
}

// ��ť������
void OnButton1Click(HWND hwnd) {
    PerformCleanOperation(hwnd);
}

void OnButton2Click(HWND hwnd) {
    PerformRestoreOperation(hwnd);
}

void OnGotoButtonClick() {
    ShowMainWindow();
}

// ״̬�����º���
void UpdateStatus(HWND hwnd, const wchar_t* text) {
    if (g_hStatus) {
        SetWindowTextW(g_hStatus, text);
    }
}

// ����Goto��ť����
void CreateGotoButton() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = GotoWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"GotoButtonClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    // ��ȡ��Ļ�ߴ�
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    g_hGotoButton = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"GotoButtonClass",
        L"",
        WS_POPUP | WS_VISIBLE,
        screenWidth - GOTO_BUTTON_WIDTH - 40, 
        screenHeight - GOTO_BUTTON_HEIGHT - 40,
        GOTO_BUTTON_WIDTH, 
        GOTO_BUTTON_HEIGHT,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    // ����"��������"��ť
    CreateWindowW(
        L"BUTTON", L"��������",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        0, 0, 100, GOTO_BUTTON_HEIGHT,
        g_hGotoButton, (HMENU)ID_GOTO_BUTTON, GetModuleHandle(NULL), NULL
    );

    // ����X�رհ�ť
    CreateWindowW(
        L"BUTTON", L"X",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        100, 0, 50, GOTO_BUTTON_HEIGHT,
        g_hGotoButton, (HMENU)ID_CLOSE_BUTTON, GetModuleHandle(NULL), NULL
    );
}

// ��ʾ������
void ShowMainWindow() {
    if (g_hMainWnd) {
        ShowWindow(g_hMainWnd, SW_SHOW);
        SetForegroundWindow(g_hMainWnd);
    }
}

// Goto��ť���ڹ���
LRESULT CALLBACK GotoWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_GOTO_BUTTON:
            OnGotoButtonClick();
            break;
        case ID_CLOSE_BUTTON:
            DestroyWindow(hwnd);
            break;
        }
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

// �����ڹ���
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_hMainWnd = hwnd;
        
        // ����״̬��
        g_hStatus = CreateWindowW(
            L"STATIC", L"����",
            WS_VISIBLE | WS_CHILD | SS_SUNKEN,
            0, 200 - STATUS_HEIGHT, 350, STATUS_HEIGHT,
            hwnd, NULL, NULL, NULL
        );

        // ��������Ԫ��
        CreateWindowW(L"STATIC", L"�����֩��Plus v2.0.0",
            WS_VISIBLE | WS_CHILD,
            80, 20, 180, 20,
            hwnd, NULL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"ɾ����֩��",
            WS_VISIBLE | WS_CHILD,
            50, 60, 100, 30,
            hwnd, (HMENU)ID_BUTTON1, NULL, NULL);

        CreateWindowW(L"BUTTON", L"�ָ���֩��",
            WS_VISIBLE | WS_CHILD,
            180, 60, 100, 30,
            hwnd, (HMENU)ID_BUTTON2, NULL, NULL);
        break;

    case WM_SIZE:
        if (g_hStatus) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            SetWindowPos(g_hStatus, NULL,
                0, rcClient.bottom - STATUS_HEIGHT,
                rcClient.right, STATUS_HEIGHT,
                SWP_NOZORDER);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BUTTON1:
            OnButton1Click(hwnd);
            break;
        case ID_BUTTON2:
            OnButton2Click(hwnd);
            break;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// ������
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ������ԱȨ��
    if (!IsUserAnAdmin()) {
        MessageBoxW(NULL, L"���Թ���Ա������д˳����Ի����������", L"Ȩ�޲���", MB_ICONWARNING);
        // ��Ȼ�������У���ĳЩ���ܿ�������
    }

    // ע����������
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EnhancedProcessManager";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"����ע��ʧ��!", L"����", MB_ICONERROR);
        return 1;
    }

    // ���������� (��ʼ����)
    g_hMainWnd = CreateWindowW(
	    wc.lpszClassName,
	    L"�����֩��Plus v2.0.0",
	    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,  // �Ƴ���WS_MINIMIZEBOX
	    CW_USEDEFAULT, CW_USEDEFAULT,
	    350, 200,
	    NULL, NULL, hInstance, NULL
	);
    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"���ڴ���ʧ��!", L"����", MB_ICONERROR);
        return 1;
    }

    // ����Goto��ť(�����򿪺͹رհ�ť)
    CreateGotoButton();

    // ��Ϣѭ��
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
