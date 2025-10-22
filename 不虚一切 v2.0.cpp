#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <shellapi.h>
#include <shlobj.h>

// 定义按钮ID
#define ID_BUTTON_RED_DEL      101
#define ID_BUTTON_RED_RESTORE  102
#define ID_BUTTON_WIND_DEL     103
#define ID_BUTTON_WIND_RESTORE 104
#define ID_GOTO_BUTTON         105
#define ID_CLOSE_BUTTON        106

// 全局变量
HWND g_hStatus = NULL;
HWND g_hMainWnd = NULL;
HWND g_hGotoButton = NULL;
const int STATUS_HEIGHT = 20;
const int GOTO_BUTTON_WIDTH = 150;
const int GOTO_BUTTON_HEIGHT = 50;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GotoWndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateStatus(HWND hwnd, const wchar_t* text);
void ShowMainWindow();
void CreateGotoButton();
bool IsUserAnAdmin();
bool KillProcessWithTaskkill(const wchar_t* processName);
bool IsProcessRunning(const wchar_t* processName);
void KillTargetProcesses(const std::vector<std::wstring>& targets);
void RefreshSystemTray();

// 红蜘蛛和大风车的配置
namespace RedSpider {
    const std::vector<std::wstring> targets = {
        L"REDAgent.exe", L"checkrs.exe", L"rscheck.exe"
    };
    const wchar_t* restorePath = L"C:\\Program Files (x86)\\3000soft\\Red Spider\\REDAgent.exe";
}

namespace Windmill {
    const std::vector<std::wstring> targets = {
        L"iConsoleApp.exe", L"iConsoleSec.exe", L"iClient.exe"
    };
    const wchar_t* restorePath = L"C:\\Program Files (x86)\\Cooltion\\Elearning\\iConsoleApp.exe";
}

// 检查管理员权限
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

// 使用taskkill命令终止进程
bool KillProcessWithTaskkill(const wchar_t* processName) {
    std::wstring command = L"taskkill /f /im \"" + std::wstring(processName) + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000); // 等待5秒
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
}

// 检查进程是否运行
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

// 终止目标进程
void KillTargetProcesses(const std::vector<std::wstring>& targets) {
    for (const auto& proc : targets) {
        // 使用taskkill命令终止进程
        if (!KillProcessWithTaskkill(proc.c_str())) {
            // 如果taskkill失败，尝试原始方法
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
        
        // 短暂延迟让系统处理
        Sleep(200);
    }
}

// 刷新系统托盘
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

// 操作函数
void PerformCleanOperation(HWND hwnd, const std::vector<std::wstring>& targets, const wchar_t* successMsg) {
    UpdateStatus(hwnd, L"正在清理进程...");
    KillTargetProcesses(targets);

    bool allStopped = true;
    for (const auto& proc : targets) {
        if (IsProcessRunning(proc.c_str())) {
            allStopped = false;
            break;
        }
    }

    if (allStopped) {
        UpdateStatus(hwnd, L"清理完成");
        MessageBoxW(NULL, successMsg, L"操作成功", MB_ICONINFORMATION);
    } else {
        UpdateStatus(hwnd, L"清理未完成");
        MessageBoxW(NULL, L"失败!\n解决办法:重试", L"操作警告", MB_ICONWARNING);
    }
    RefreshSystemTray();
}

void PerformRestoreOperation(const wchar_t* exePath) {
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        MessageBoxW(NULL, L"程序已成功启动", L"操作成功", MB_ICONINFORMATION);
    } else {
        MessageBoxW(NULL, L"程序启动失败", L"操作错误", MB_ICONERROR);
    }
}

// 按钮处理函数
void OnRedDeleteClick(HWND hwnd) {
    PerformCleanOperation(hwnd, RedSpider::targets, 
        L"少年，本就是一片不被定义的海\n          祝你玩机愉快!");
}

void OnRedRestoreClick() {
    PerformRestoreOperation(RedSpider::restorePath);
}

void OnWindDeleteClick(HWND hwnd) {
    PerformCleanOperation(hwnd, Windmill::targets, 
        L"少年，本就是一片不被定义的海\n          祝你玩机愉快!");
}

void OnWindRestoreClick() {
    PerformRestoreOperation(Windmill::restorePath);
}

void OnGotoButtonClick() {
    ShowMainWindow();
}

// 状态栏更新函数
void UpdateStatus(HWND hwnd, const wchar_t* text) {
    if (g_hStatus) {
        SetWindowTextW(g_hStatus, text);
    }
}

// 创建Goto按钮窗口
void CreateGotoButton() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = GotoWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"GotoButtonClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    // 获取屏幕尺寸
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

    // 创建"打开主程序"按钮
    CreateWindowW(
        L"BUTTON", L"打开主程序",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        0, 0, 100, GOTO_BUTTON_HEIGHT,
        g_hGotoButton, (HMENU)ID_GOTO_BUTTON, GetModuleHandle(NULL), NULL
    );

    // 创建X关闭按钮
    CreateWindowW(
        L"BUTTON", L"X",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        100, 0, 50, GOTO_BUTTON_HEIGHT,
        g_hGotoButton, (HMENU)ID_CLOSE_BUTTON, GetModuleHandle(NULL), NULL
    );
}

// 显示主窗口
void ShowMainWindow() {
    if (g_hMainWnd) {
        ShowWindow(g_hMainWnd, SW_SHOW);
        SetForegroundWindow(g_hMainWnd);
    }
}

// Goto按钮窗口过程
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

// 主窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_hMainWnd = hwnd;
        
        // 创建状态栏
        g_hStatus = CreateWindowW(
            L"STATIC", L"就绪",
            WS_VISIBLE | WS_CHILD | SS_SUNKEN,
            0, 250 - STATUS_HEIGHT, 350, STATUS_HEIGHT,
            hwnd, NULL, NULL, NULL
        );

        // 创建界面元素
        CreateWindowW(L"STATIC", L"不虚一切 v2.0",
            WS_VISIBLE | WS_CHILD,
            100, 10, 150, 20,
            hwnd, NULL, NULL, NULL);

        // 第一排按钮
        CreateWindowW(L"BUTTON", L"删除红蜘蛛",
            WS_VISIBLE | WS_CHILD,
            30, 50, 120, 30,
            hwnd, (HMENU)ID_BUTTON_RED_DEL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"恢复红蜘蛛",
            WS_VISIBLE | WS_CHILD,
            180, 50, 120, 30,
            hwnd, (HMENU)ID_BUTTON_RED_RESTORE, NULL, NULL);

        // 第二排按钮
        CreateWindowW(L"BUTTON", L"删除大风车",
            WS_VISIBLE | WS_CHILD,
            30, 100, 120, 30,
            hwnd, (HMENU)ID_BUTTON_WIND_DEL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"恢复大风车",
            WS_VISIBLE | WS_CHILD,
            180, 100, 120, 30,
            hwnd, (HMENU)ID_BUTTON_WIND_RESTORE, NULL, NULL);
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
        case ID_BUTTON_RED_DEL:    OnRedDeleteClick(hwnd); break;
        case ID_BUTTON_RED_RESTORE: OnRedRestoreClick(); break;
        case ID_BUTTON_WIND_DEL:   OnWindDeleteClick(hwnd); break;
        case ID_BUTTON_WIND_RESTORE: OnWindRestoreClick(); break;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 检查管理员权限
    if (!IsUserAnAdmin()) {
        MessageBoxW(NULL, L"请以管理员身份运行此程序以获得完整功能", L"权限不足", MB_ICONWARNING);
        // 仍然继续运行，但某些功能可能受限
    }

    // 注册主窗口类
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"EnhancedProcessManager";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"窗口注册失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建主窗口 (初始隐藏)
    g_hMainWnd = CreateWindowW(
        wc.lpszClassName,
        L"不虚一切 v2.0",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        350, 200,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建Goto按钮(包含打开和关闭按钮)
    CreateGotoButton();

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
