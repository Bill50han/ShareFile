#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <tlhelp32.h>
#include <Commctrl.h>
#pragma comment(lib, "Shcore.lib")
#include <Shobjidl.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <locale>
#include <vector>
#include <algorithm>
#include <string>
#include <functional>
#include <map>
#include <atomic>

#include "wUGComm.h"
#include "../wU-CLI/USFLog.h"
#include "wUGManager.h"

Logger logger = Logger(false, true, "glog.txt");    //���նˣ���ӡ���ļ�


LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawPlayButton(DRAWITEMSTRUCT* pDIS);
void DrawControlButton(DRAWITEMSTRUCT* pDIS, const wchar_t* c);
void ShowAdaptersOnComboBox(HWND hC);

HINSTANCE hInst;
HWND hComboAdapter;
HWND hListPath;

#define IDC_BTN_CLOSE 0x1001
#define IDC_BTN_MIN 0x1002
#define IDC_PLAY_BUTTON 0x001
#define IDC_ADAPTER_COMBOBOX 0x101
#define IDC_PATH_LISTBOX 0x201
#define IDC_PATH_LISTBOX_ADD 0x202
#define IDC_PATH_LISTBOX_DEL 0x203

HFONT g_hFont = NULL;

std::hash<std::wstring> sHash;
std::map<size_t, std::wstring> PathMap;
std::atomic_bool running = false;

void InitGlobalFont()
{
    LOGFONT lf = { 0 };
    lf.lfHeight = -12;                     // �߶ȣ�������ʾ���ַ��߶�ƥ�䣩
    lf.lfWeight = FW_NORMAL;               // ���أ�������ϸ��
    lf.lfCharSet = GB2312_CHARSET;         // �ַ��������ģ�
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"΢���ź�"); // ��������

    g_hFont = CreateFontIndirect(&lf);
    if (!g_hFont)
    {
        MessageBox(NULL, L"���崴��ʧ�ܣ�", L"����", MB_ICONERROR);
    }
}
void SetChildFont(HWND hParent, HFONT hFont)
{
    HWND hChild = GetWindow(hParent, GW_CHILD);
    while (hChild)
    {
        SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

std::wstring Dos2NTPath(const std::wstring inpt)
{
    WCHAR deviceName[5] = { 0 };
    WCHAR target[50] = { 0 };
    deviceName[0] = inpt[0];
    deviceName[1] = inpt[1];
    QueryDosDeviceW(deviceName, target, 49);
    return target + inpt.substr(2);
}

BOOL IsProcessRunning(const WCHAR* processName) {
    BOOL exists = FALSE;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    // �������̿���
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log("���մ���ʧ�ܣ�����: %X\n", GetLastError());
        return FALSE;
    }

    // ���������б�
    if (Process32FirstW(snapshot, &entry)) {
        do {
            // �ȽϽ������������ִ�Сд��
            if (wcscmp(entry.szExeFile, processName) == 0) {
                Log("�ҵ�����");
                exists = TRUE;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return exists;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    setlocale(LC_ALL, "");
    InitGlobalFont();
    SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);

    std::wfstream loadpath("path.conf", std::ios::in);
    if (loadpath.is_open())
    {
        loadpath.imbue(std::locale(std::locale(), "", LC_CTYPE));

        while (1)
        {
            std::wstring t;
            size_t h = 0;

            loadpath >> h;
            if (h == 0 || loadpath.eof() || loadpath.peek() == EOF)
            {
                break;
            }
            loadpath.ignore();
            std::getline(loadpath, t);
            PathMap[h] = t;
        }

        loadpath.flush();
        loadpath.close();
    }

    if (IsProcessRunning(L"wU-CLI.exe"))
    {
        running = true;
    }

    WNDCLASSW wc = { 0 };

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    //wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PROJECT1));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    //wc.lpszMenuName = MAKEINTRESOURCEW(IDC_PROJECT1);
    wc.lpszClassName = L"ShareFile";
    //wc.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassW(&wc);

    hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����

    HWND hWnd = CreateWindowW(L"ShareFile", L"ShareFile", WS_POPUP | WS_VISIBLE, 200, 150, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        //if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DeleteObject(g_hFont);

    std::wfstream savepath("path.conf", std::ios::out | std::ios::trunc);
    savepath.imbue(std::locale(std::locale(), "", LC_CTYPE));
    for (const auto& i : PathMap)
    {
        savepath << i.first << L" " << i.second << L"\n";
    }
    savepath << 0;
    savepath.flush();
    savepath.close();

    Log("GUI �˳�: %X", (int)msg.wParam);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateWindowW(
            L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            770, 0, 30, 20,
            hWnd, (HMENU)IDC_BTN_CLOSE, hInst, NULL
        );
        CreateWindowW(
            L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            740, 0, 30, 20,
            hWnd, (HMENU)IDC_BTN_MIN, hInst, NULL
        );

        CreateWindowW(
            L"BUTTON",                   // ��ť��
            L"",                         // ��ť�ı������գ�
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,  // �ؼ���BS_OWNERDRAW
            20, 40, 80, 80,         // ��ťλ�úʹ�С
            hWnd,                  // �����ھ��
            (HMENU)IDC_PLAY_BUTTON,      // ��ťID
            hInst,                   // ����ʵ�����
            NULL
        );
        hComboAdapter = CreateWindowW(
            L"COMBOBOX",                     // �ؼ�����
            L"",                             // ��ʼ�ı�
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,  // ��ʽ
            160, 60,                            // λ��
            600, 90,                   // ��С���߶�Ϊ�������ܸ߶ȣ�
            hWnd,                      // �����ھ��
            (HMENU)IDC_ADAPTER_COMBOBOX,            // �ؼ�ID
            hInst,                       // ����ʵ�����
            NULL
        );
        ComboBox_SetCueBannerText(hComboAdapter, L"��ѡ��������������");
        ShowAdaptersOnComboBox(hComboAdapter);

        hListPath = CreateWindowExW(
            WS_EX_CLIENTEDGE | WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
            L"LISTBOX",                     // �ؼ�����
            L"",                             // ��ʼ�ı�
            LBS_NOTIFY | LBS_HASSTRINGS | WS_CHILD | WS_VISIBLE | WS_VSCROLL,  // ��ʽ
            160, 130,                            // λ��
            600, 420,                   // ��С���߶�Ϊ�������ܸ߶ȣ�
            hWnd,                      // �����ھ��
            (HMENU)IDC_PATH_LISTBOX,            // �ؼ�ID
            hInst,                       // ����ʵ�����
            NULL
        );

        CreateWindowW(
            L"BUTTON", L"+",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            690, 550, 30, 30,
            hWnd, (HMENU)IDC_PATH_LISTBOX_ADD, hInst, NULL
        );

        CreateWindowW(
            L"BUTTON", L"-",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            730, 550, 30, 30,
            hWnd, (HMENU)IDC_PATH_LISTBOX_DEL, hInst, NULL
        );

        if (PathMap.size() > 0)
        {
            for (const auto& i : PathMap)
            {
                ListBox_AddString(hListPath, i.second.c_str());
            }
        }

        std::wstring buffer;
        std::wfstream o("adapter.conf", std::ios::in);
        if (o.is_open())
        {
            o.imbue(std::locale(std::locale(), "", LC_CTYPE));
            std::getline(o, buffer);
            o.close();

            int ind = ComboBox_FindString(hComboAdapter, -1, buffer.c_str());
            if (ind != CB_ERR)
            {
                ComboBox_SetCurSel(hComboAdapter, ind);
            }
        }

        SetChildFont(hWnd, g_hFont);
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_BTN_MIN:
            ShowWindow(hWnd, SW_MINIMIZE); // ��С��
            break;
        case IDC_BTN_CLOSE:
            DestroyWindow(hWnd); // �رմ���
            break;
        case IDC_ADAPTER_COMBOBOX:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int index = ComboBox_GetCurSel(hComboAdapter);
                if (index != CB_ERR)
                {
                    WCHAR* buffer = new WCHAR[ComboBox_GetLBTextLen(hComboAdapter, index) + 1];
                    ComboBox_GetLBText(hComboAdapter, index, buffer);
                    std::wfstream o = std::wfstream("adapter.conf", std::ios::out);
                    o.imbue(std::locale(std::locale(), "", LC_CTYPE));
                    o << buffer;
                    o.close();
                    delete[] buffer;
                }
            }
            break;
        }
        case IDC_PATH_LISTBOX_ADD:
        {
            IFileOpenDialog* pfd = NULL;
            CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
            DWORD dwFlags;
            pfd->GetOptions(&dwFlags);
            pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT);
            COMDLG_FILTERSPEC fileType[] =
            {

                 { L"��������", L"*.*" }
            };
            pfd->SetFileTypes(ARRAYSIZE(fileType), fileType);
            pfd->SetFileTypeIndex(1);
            HRESULT hr = pfd->Show(NULL);
            if (!SUCCEEDED(hr)) break;
            IShellItemArray* pSelItemArray;
            pfd->GetResults(&pSelItemArray);

            DWORD num = 0;
            pSelItemArray->GetCount(&num);

            for (size_t i = 0; i < num; i++)
            {
                LPWSTR pszFilePath = NULL;
                IShellItem* pSelItem;
                pSelItemArray->GetItemAt(i, &pSelItem);
                pSelItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEEDITING, &pszFilePath);

                if (ListBox_FindString(hListPath, -1, pszFilePath) == LB_ERR)
                {
                    ListBox_AddString(hListPath, pszFilePath);
                    PathMap[sHash(pszFilePath)] = pszFilePath;

                    std::wstring p = Dos2NTPath(pszFilePath);

                    PMessage msg = (PMessage)malloc(sizeof(Message) + (wcslen(p.c_str()) + 1) * sizeof(wchar_t));
                    if (msg == NULL) break;
                    msg->size = sizeof(Message) + (wcslen(p.c_str()) + 1) * sizeof(wchar_t);
                    msg->type = M_ADD_PATH;
                    msg->AddPath.hash = sHash(p.c_str());
                    msg->AddPath.length = wcslen(p.c_str()) * sizeof(wchar_t);
                    wcscpy(msg->AddPath.path, p.c_str());

                    UCommunication::GetInstance().SendUCommMessage(msg);
                    free(msg);
                }

                CoTaskMemFree(pszFilePath);
                pSelItem->Release();
            }
            pSelItemArray->Release();
            pfd->Release();

            break;
        }
        case IDC_PATH_LISTBOX_DEL:
        {
            int index = ListBox_GetCurSel(hListPath);
            if (index != LB_ERR)
            {
                WCHAR* buffer = new WCHAR[ListBox_GetTextLen(hListPath, index) + 1];
                memset(buffer, 0, (ListBox_GetTextLen(hListPath, index) + 1) * 2);
                ListBox_GetText(hListPath, index, buffer);
                std::wstring p = Dos2NTPath(buffer);
                PathMap.erase(sHash(p.c_str()));
                PMessage msg = (PMessage)malloc(sizeof(Message));
                if (msg == NULL) break;
                msg->size = sizeof(Message);
                msg->type = M_DEL_PATH;
                msg->DelPath.hash = sHash(p.c_str());
                UCommunication::GetInstance().SendUCommMessage(msg);
                free(msg);
                delete[] buffer;

                ListBox_DeleteString(hListPath, index);
            }

            break;
        }
        case IDC_PLAY_BUTTON:
        {
            WatchDog& inst = WatchDog::GetInstance();
            if (!running)
            {
                inst.InstallDriver();
                inst.StartDriver();
                Sleep(100);
                DWORD r = inst.RunAndGetExitCode(L"wU-CLI.exe");
                if (r != STILL_ACTIVE)
                {
                    switch (r)
                    {
                    case 1:
                        MessageBoxW(NULL, L"�޷��������ں˵����ӣ����������ٴγ���", L"ϵͳ��һЩС����", MB_OK);
                        break;
                    case 2:
                        MessageBoxW(NULL, L"��ȷ����ѡ������ȷ����������������״������", L"������һЩС����", MB_OK);
                        break;
                    default:
                        MessageBoxW(NULL, L"��ȷ�ϵ��Ե�����״������", L"������һЩС����", MB_OK);
                        break;
                    }
                    inst.StopDriver();
                    break;
                }
                
                UCommunication::GetInstance().Start();

                if (PathMap.size() > 0)
                {
                    for (const auto& i : PathMap)
                    {
                        std::wstring p = Dos2NTPath(i.second);

                        PMessage msg = (PMessage)malloc(sizeof(Message) + (wcslen(p.c_str()) + 1) * sizeof(wchar_t));
                        if (msg == NULL) break;
                        msg->size = sizeof(Message) + (wcslen(p.c_str()) + 1) * sizeof(wchar_t);
                        msg->type = M_ADD_PATH;
                        msg->AddPath.hash = i.first;
                        msg->AddPath.length = wcslen(p.c_str()) * sizeof(wchar_t);
                        wcscpy(msg->AddPath.path, p.c_str());

                        UCommunication::GetInstance().SendUCommMessage(msg);
                        free(msg);
                    }
                }

                running = true;
            }
            else
            {
                inst.StopDriver();
                inst.TerminateU();
                UCommunication::GetInstance().Stop();

                running = false;
            }

            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HBRUSH hBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
        RECT rect = { 0, 0, 120, 600 };
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
        if (pDIS->CtlID == IDC_PLAY_BUTTON)
        {
            DrawPlayButton(pDIS);
            return TRUE;
        }
        if (pDIS->CtlID == IDC_BTN_CLOSE)
        {
            DrawControlButton(pDIS, L"��");
            return TRUE;
        }
        if (pDIS->CtlID == IDC_BTN_MIN)
        {
            DrawControlButton(pDIS, L"-");
            return TRUE;
        }
        break;
    }
    case WM_NCHITTEST:
    {
        // Ĭ�Ϸ��� HTCLIENT���������϶�
        LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
        if (hit == HTCLIENT) {
            return HTCAPTION; // ���ͻ��������Ϊ������
        }
        //���߿���Ϊ���壬���ɵ������ڴ�С
        const static std::vector<LRESULT> resizeHits = { HTLEFT, HTRIGHT, HTTOP, HTBOTTOM, HTTOPLEFT, HTTOPRIGHT, HTBOTTOMLEFT, HTBOTTOMRIGHT };
        if (std::find(resizeHits.begin(), resizeHits.end(), hit) != resizeHits.end()) {
            return HTBORDER;
        }
        return hit;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void DrawPlayButton(DRAWITEMSTRUCT* pDIS) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    HBRUSH hBrushBg = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
    FillRect(hdc, &rc, hBrushBg);
    DeleteObject(hBrushBg);

    if (running)
    {
        HBRUSH hBrRed;
        HPEN hPen;
        HBRUSH hBrWhite;
        if (pDIS->itemState & ODS_SELECTED)
        {
            hBrRed = CreateSolidBrush(RGB(0xBA, 0x39, 0x24));
            hPen = CreatePen(PS_SOLID, 1, RGB(0xF3, 0xF3, 0xF3));
            hBrWhite = CreateSolidBrush(RGB(0xBF, 0xBF, 0xBF));

        }
        else
        {
            hBrRed = CreateSolidBrush(RGB(0xFA, 0x4E, 0x32));
            hPen = CreatePen(PS_SOLID, 1, RGB(0xF3, 0xF3, 0xF3));
            hBrWhite = CreateSolidBrush(RGB(255, 255, 255));
        }


        SelectObject(hdc, hBrRed);
        SelectObject(hdc, hPen);
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom); // ����Բ��

        ULONG centerX = (rc.left + rc.right) / 2.0;
        ULONG centerY = (rc.top + rc.bottom) / 2.0;
        ULONG blockSize = 13;
        RECT block = { centerX - blockSize, centerY - blockSize,centerX + blockSize, centerY + blockSize };
        FillRect(hdc, &block, hBrWhite);

        DeleteObject(hBrRed);
        DeleteObject(hBrWhite);
        DeleteObject(hPen);
    }
    else
    {
        // 1. ������ɫԲ�α���
        HBRUSH hBrGreen = CreateSolidBrush(RGB(0x22, 0xB1, 0x4C)); // ��ɫ��ˢ
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0xF3, 0xF3, 0xF3));

        SelectObject(hdc, hBrGreen);
        SelectObject(hdc, hPen);
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom); // ����Բ��

        // 2. ���ư�ɫ�����Σ�����ͼ�꣩
        ULONG centerX = (rc.left + rc.right) / 2.0;
        ULONG centerY = (rc.top + rc.bottom) / 2.0;
        ULONG triangleSize = 9; // �����δ�С

        POINT pts[3] = {
            {centerX - triangleSize * 1.3, centerY - triangleSize * 1.732}, // �󶥵�
            {centerX - triangleSize * 1.3, centerY + triangleSize * 1.732}, // �¶���
            {centerX + triangleSize * 1.7, centerY}                  // �Ҷ���
        };

        HBRUSH hBrWhite = CreateSolidBrush(RGB(255, 255, 255)); // ��ɫ���
        SelectObject(hdc, hBrWhite);
        Polygon(hdc, pts, 3); // ����������

        // 3. ����ť״̬������/���㣩
        if (pDIS->itemState & ODS_SELECTED) // ��ť������ʱ��ɫ�䰵
        {
            HBRUSH hBrDarkGreen = CreateSolidBrush(RGB(0x18, 0x84, 0x39));
            SelectObject(hdc, hBrDarkGreen);
            Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
            HBRUSH hBrDarkWhite = CreateSolidBrush(RGB(0xBF, 0xBF, 0xBF));
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0xBF, 0xBF, 0xBF));
            SelectObject(hdc, hBrDarkWhite);
            SelectObject(hdc, hPen);
            Polygon(hdc, pts, 3); // ����������
            DeleteObject(hBrDarkGreen);
            DeleteObject(hBrDarkWhite);
            DeleteObject(hPen);
        }

        // 4. ������Դ
        DeleteObject(hBrGreen);
        DeleteObject(hBrWhite);
        DeleteObject(hPen);
    }

}

void DrawControlButton(DRAWITEMSTRUCT* pDIS, const wchar_t* c)
{
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));

    if (L'��' == c[0])
    {
        HBRUSH hBrRed = CreateSolidBrush(RGB(0xFA, 0x4E, 0x32));
        FillRect(hdc, &rc, hBrRed);
        DeleteObject(hBrRed);
    }

    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, c, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void ShowAdaptersOnComboBox(HWND hC)
{
    PIP_ADAPTER_ADDRESSES adatpers = nullptr;
    ULONG size = 0;
    GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &size);
    adatpers = (PIP_ADAPTER_ADDRESSES)malloc(size);
    GetAdaptersAddresses(AF_INET, 0, NULL, adatpers, &size);

    for (PIP_ADAPTER_ADDRESSES i = adatpers; i != NULL; i = i->Next)
    {
        if (i->OperStatus != IfOperStatusUp) continue;

        ComboBox_AddString(hC, i->FriendlyName);
    }

    free(adatpers);
}
