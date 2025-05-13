#pragma once

#include "../wU-CLI/USFLog.h"

#include <windows.h>
#include <string>
#include <thread>

class WatchDog
{
private:
    WatchDog() = default;
    WatchDog(const WatchDog&) = delete;
    WatchDog& operator=(const WatchDog&) = delete;

    PROCESS_INFORMATION pi = { 0 };

    const char* lpszDriverName = "wKernel";
    const char* lpszDriverPath = "wKernel.sys";
    const char* lpszAltitude = "136650.5";

public:
    static WatchDog& GetInstance()
    {
        static WatchDog instance;
        return instance;
    }

    DWORD RunAndGetExitCode(const std::wstring& path, DWORD waitTime = 100)
    {
        Log("����cli");
        STARTUPINFO si = { sizeof(si) };

        if (!CreateProcessW(
            NULL,
            (LPWSTR)path.c_str(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi))
        {
            return GetLastError() + 0x10000; // ���ش���ʧ�ܴ�����
        }

        DWORD exitCode = STILL_ACTIVE;

        // ���ټ������Ƿ����
        if (WaitForSingleObject(pi.hProcess, waitTime) == WAIT_OBJECT_0)
        {
            GetExitCodeProcess(pi.hProcess, &exitCode);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode;
    }

    BOOL TerminateU()
    {
        Log("�ر�cli");
        HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"wu-cli-should-close");
        BOOL r = FALSE;
        if (hEvent) {
            r = SetEvent(hEvent);
            CloseHandle(hEvent);
        }
        return r;
    }

    BOOL InstallDriver()
    {
        Log("��ʼ��װ wKernel");
        char    szTempStr[MAX_PATH];
        HKEY    hKey;
        DWORD    dwData;
        char    szDriverImagePath[MAX_PATH];

        if (NULL == lpszDriverName || NULL == lpszDriverPath)
        {
            return FALSE;
        }
        //�õ�����������·��
        GetFullPathNameA(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

        SC_HANDLE hServiceMgr = NULL;// SCM�������ľ��
        SC_HANDLE hService = NULL;// NT��������ķ�����

        //�򿪷�����ƹ�����
        hServiceMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hServiceMgr == NULL)
        {
            Log("OpenSCManagerʧ�� 0x%X", GetLastError());
            CloseServiceHandle(hServiceMgr);
            return FALSE;
        }

        Log("OpenSCManager�ɹ�");

        //������������Ӧ�ķ���
        hService = CreateServiceA(hServiceMgr,
            lpszDriverName,             // �����������ע����е�����
            lpszDriverName,             // ע������������DisplayName ֵ
            SERVICE_ALL_ACCESS,         // ������������ķ���Ȩ��
            SERVICE_FILE_SYSTEM_DRIVER, // ��ʾ���صķ������ļ�ϵͳ��������
            SERVICE_DEMAND_START,       // ע������������Start ֵ
            SERVICE_ERROR_IGNORE,       // ע������������ErrorControl ֵ
            szDriverImagePath,          // ע������������ImagePath ֵ
            "FSFilter Activity Monitor",// ע������������Group ֵ
            NULL,
            "FltMgr",                   // ע������������DependOnService ֵ
            NULL,
            NULL);

        if (hService == NULL)
        {
            if (GetLastError() == ERROR_SERVICE_EXISTS)
            {
                Log("���񴴽�ʧ�ܣ������ڷ����Ѿ�������");
                CloseServiceHandle(hService);       // ������
                CloseServiceHandle(hServiceMgr);    // SCM���
                return TRUE;
            }
            else
            {
                Log("���񴴽�ʧ�� 0x%X", GetLastError());
                CloseServiceHandle(hService);       // ������
                CloseServiceHandle(hServiceMgr);    // SCM���
                return FALSE;
            }
        }
        CloseServiceHandle(hService);       // ������
        CloseServiceHandle(hServiceMgr);    // SCM���
        Log("CreateService�ɹ�");

        //-------------------------------------------------------------------------------------------------------
        // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances�ӽ��µļ�ֵ�� 
        //-------------------------------------------------------------------------------------------------------
        strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, "\\Instances");
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, szTempStr, 0, (char*)"", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            Log("����ע�������ʧ��");
            return FALSE;
        }
        // ע������������DefaultInstance ֵ 
        strcpy(szTempStr, lpszDriverName);
        strcat(szTempStr, " Instance");
        if (RegSetValueExA(hKey, "DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
        {
            Log("����ע���Instance��ʧ��");
            return FALSE;
        }
        RegFlushKey(hKey);//ˢ��ע���
        RegCloseKey(hKey);
        //-------------------------------------------------------------------------------------------------------

        //-------------------------------------------------------------------------------------------------------
        // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance�ӽ��µļ�ֵ�� 
        //-------------------------------------------------------------------------------------------------------
        strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, "\\Instances\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, " Instance");
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, szTempStr, 0, (char*)"", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            Log("����ע���Instances\\DriverName��ʧ��");
            return FALSE;
        }
        // ע������������Altitude ֵ
        strcpy(szTempStr, lpszAltitude);
        if (RegSetValueExA(hKey, "Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
        {
            Log("����ע���Altitude��ʧ��");
            return FALSE;
        }
        // ע������������Flags ֵ
        dwData = 0x0;
        if (RegSetValueExA(hKey, "Flags", 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            Log("����ע���Flag��ʧ��");
            return FALSE;
        }
        RegFlushKey(hKey);//ˢ��ע���
        RegCloseKey(hKey);
        //-------------------------------------------------------------------------------------------------------

        Log("wKernel ��װ�ɹ�");

        return TRUE;
    }

    BOOL StartDriver()
    {
        Log("��ʼ���� wKernel");
        SC_HANDLE        schManager;
        SC_HANDLE        schService;
        SERVICE_STATUS    svcStatus;

        if (NULL == lpszDriverName)
        {
            return FALSE;
        }

        schManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            Log("OpenSCManager���� 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService���� 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }

        if (!StartServiceW(schService, 0, NULL))
        {
            Log("����wKernel����ʧ�� 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
            {
                // �����Ѿ�����
                return TRUE;
            }
            return FALSE;
        }

        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel �����ɹ�");

        return TRUE;
    }

    BOOL StopDriver()
    {
        Log("��ʼֹͣ wKernel");
        SC_HANDLE        schManager;
        SC_HANDLE        schService;
        SERVICE_STATUS    svcStatus;
        bool            bStopped = false;

        schManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            Log("OpenSCManager���� 0x%X", GetLastError());
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService���� 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
        {
            Log("ControlService���� 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }

        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel ֹͣ�ɹ�");

        return TRUE;
    }

    BOOL DeleteDriver()
    {
        Log("��ʼж�� wKernel");
        SC_HANDLE        schManager;
        SC_HANDLE        schService;
        SERVICE_STATUS    svcStatus;

        schManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            Log("OpenSCManager���� 0x%X", GetLastError());
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService���� 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
        if (!DeleteService(schService))
        {
            Log("DeleteService���� 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel ж�سɹ�");

        return TRUE;
    }
};