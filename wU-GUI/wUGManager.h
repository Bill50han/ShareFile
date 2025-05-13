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
        Log("启动cli");
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
            return GetLastError() + 0x10000; // 返回创建失败错误码
        }

        DWORD exitCode = STILL_ACTIVE;

        // 快速检测程序是否崩溃
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
        Log("关闭cli");
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
        Log("开始安装 wKernel");
        char    szTempStr[MAX_PATH];
        HKEY    hKey;
        DWORD    dwData;
        char    szDriverImagePath[MAX_PATH];

        if (NULL == lpszDriverName || NULL == lpszDriverPath)
        {
            return FALSE;
        }
        //得到完整的驱动路径
        GetFullPathNameA(lpszDriverPath, MAX_PATH, szDriverImagePath, NULL);

        SC_HANDLE hServiceMgr = NULL;// SCM管理器的句柄
        SC_HANDLE hService = NULL;// NT驱动程序的服务句柄

        //打开服务控制管理器
        hServiceMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hServiceMgr == NULL)
        {
            Log("OpenSCManager失败 0x%X", GetLastError());
            CloseServiceHandle(hServiceMgr);
            return FALSE;
        }

        Log("OpenSCManager成功");

        //创建驱动所对应的服务
        hService = CreateServiceA(hServiceMgr,
            lpszDriverName,             // 驱动程序的在注册表中的名字
            lpszDriverName,             // 注册表驱动程序的DisplayName 值
            SERVICE_ALL_ACCESS,         // 加载驱动程序的访问权限
            SERVICE_FILE_SYSTEM_DRIVER, // 表示加载的服务是文件系统驱动程序
            SERVICE_DEMAND_START,       // 注册表驱动程序的Start 值
            SERVICE_ERROR_IGNORE,       // 注册表驱动程序的ErrorControl 值
            szDriverImagePath,          // 注册表驱动程序的ImagePath 值
            "FSFilter Activity Monitor",// 注册表驱动程序的Group 值
            NULL,
            "FltMgr",                   // 注册表驱动程序的DependOnService 值
            NULL,
            NULL);

        if (hService == NULL)
        {
            if (GetLastError() == ERROR_SERVICE_EXISTS)
            {
                Log("服务创建失败，是由于服务已经创立过");
                CloseServiceHandle(hService);       // 服务句柄
                CloseServiceHandle(hServiceMgr);    // SCM句柄
                return TRUE;
            }
            else
            {
                Log("服务创建失败 0x%X", GetLastError());
                CloseServiceHandle(hService);       // 服务句柄
                CloseServiceHandle(hServiceMgr);    // SCM句柄
                return FALSE;
            }
        }
        CloseServiceHandle(hService);       // 服务句柄
        CloseServiceHandle(hServiceMgr);    // SCM句柄
        Log("CreateService成功");

        //-------------------------------------------------------------------------------------------------------
        // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances子健下的键值项 
        //-------------------------------------------------------------------------------------------------------
        strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, "\\Instances");
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, szTempStr, 0, (char*)"", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            Log("创建注册表主键失败");
            return FALSE;
        }
        // 注册表驱动程序的DefaultInstance 值 
        strcpy(szTempStr, lpszDriverName);
        strcat(szTempStr, " Instance");
        if (RegSetValueExA(hKey, "DefaultInstance", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
        {
            Log("创建注册表Instance项失败");
            return FALSE;
        }
        RegFlushKey(hKey);//刷新注册表
        RegCloseKey(hKey);
        //-------------------------------------------------------------------------------------------------------

        //-------------------------------------------------------------------------------------------------------
        // SYSTEM\\CurrentControlSet\\Services\\DriverName\\Instances\\DriverName Instance子健下的键值项 
        //-------------------------------------------------------------------------------------------------------
        strcpy(szTempStr, "SYSTEM\\CurrentControlSet\\Services\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, "\\Instances\\");
        strcat(szTempStr, lpszDriverName);
        strcat(szTempStr, " Instance");
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, szTempStr, 0, (char*)"", TRUE, KEY_ALL_ACCESS, NULL, &hKey, (LPDWORD)&dwData) != ERROR_SUCCESS)
        {
            Log("创建注册表Instances\\DriverName项失败");
            return FALSE;
        }
        // 注册表驱动程序的Altitude 值
        strcpy(szTempStr, lpszAltitude);
        if (RegSetValueExA(hKey, "Altitude", 0, REG_SZ, (CONST BYTE*)szTempStr, (DWORD)strlen(szTempStr)) != ERROR_SUCCESS)
        {
            Log("创建注册表Altitude项失败");
            return FALSE;
        }
        // 注册表驱动程序的Flags 值
        dwData = 0x0;
        if (RegSetValueExA(hKey, "Flags", 0, REG_DWORD, (CONST BYTE*) & dwData, sizeof(DWORD)) != ERROR_SUCCESS)
        {
            Log("创建注册表Flag项失败");
            return FALSE;
        }
        RegFlushKey(hKey);//刷新注册表
        RegCloseKey(hKey);
        //-------------------------------------------------------------------------------------------------------

        Log("wKernel 安装成功");

        return TRUE;
    }

    BOOL StartDriver()
    {
        Log("开始启动 wKernel");
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
            Log("OpenSCManager错误 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService错误 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }

        if (!StartServiceW(schService, 0, NULL))
        {
            Log("启动wKernel服务失败 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
            {
                // 服务已经开启
                return TRUE;
            }
            return FALSE;
        }

        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel 启动成功");

        return TRUE;
    }

    BOOL StopDriver()
    {
        Log("开始停止 wKernel");
        SC_HANDLE        schManager;
        SC_HANDLE        schService;
        SERVICE_STATUS    svcStatus;
        bool            bStopped = false;

        schManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            Log("OpenSCManager错误 0x%X", GetLastError());
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService错误 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        if (!ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus) && (svcStatus.dwCurrentState != SERVICE_STOPPED))
        {
            Log("ControlService错误 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }

        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel 停止成功");

        return TRUE;
    }

    BOOL DeleteDriver()
    {
        Log("开始卸载 wKernel");
        SC_HANDLE        schManager;
        SC_HANDLE        schService;
        SERVICE_STATUS    svcStatus;

        schManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (NULL == schManager)
        {
            Log("OpenSCManager错误 0x%X", GetLastError());
            return FALSE;
        }
        schService = OpenServiceA(schManager, lpszDriverName, SERVICE_ALL_ACCESS);
        if (NULL == schService)
        {
            Log("OpenService错误 0x%X", GetLastError());
            CloseServiceHandle(schManager);
            return FALSE;
        }
        ControlService(schService, SERVICE_CONTROL_STOP, &svcStatus);
        if (!DeleteService(schService))
        {
            Log("DeleteService错误 0x%X", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schManager);
            return FALSE;
        }
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);

        Log("wKernel 卸载成功");

        return TRUE;
    }
};