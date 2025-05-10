#pragma once

#include "../wU-CLI/USFLog.h"

#include <windows.h>
#include <string>
#include <thread>

class WatchDog
{
public:
    static WatchDog& GetInstance()
    {
        static WatchDog instance;
        return instance;
    }

    DWORD RunAndGetExitCode(const std::wstring& path, DWORD waitTime = 100)
    {
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
        return TerminateProcess(pi.hProcess, 0);
    }

    BOOL CreateDriverService(const std::wstring& serviceName, const std::wstring& driverPath)
    {
        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
        if (!scm)
        {
            return FALSE;
        }

        SC_HANDLE service = CreateServiceW(
            scm,
            serviceName.c_str(),
            serviceName.c_str(),
            SERVICE_ALL_ACCESS,
            SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL,
            driverPath.c_str(),
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);

        BOOL result = TRUE;
        if (!service)
        {
            result = (GetLastError() == ERROR_SERVICE_EXISTS);
        }

        if (service)
        {
            CloseServiceHandle(service);
        }
        CloseServiceHandle(scm);
        return result;
    }

    BOOL StartDriver(const std::wstring& serviceName)
    {
        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (!scm)
        {
            return FALSE;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_START);
        if (!service)
        {
            CloseServiceHandle(scm);
            return FALSE;
        }

        BOOL result = StartService(service, 0, NULL);
        DWORD lastError = GetLastError();

        // 服务已经启动的情况也算成功
        if (!result && lastError == ERROR_SERVICE_ALREADY_RUNNING)
        {
            result = TRUE;
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return result;
    }

    BOOL StopDriver(const std::wstring& serviceName, DWORD timeout = 30000)
    {
        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (!scm)
        {
            return FALSE;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (!service)
        {
            CloseServiceHandle(scm);
            return FALSE;
        }

        SERVICE_STATUS status;
        BOOL result = ControlService(service, SERVICE_CONTROL_STOP, &status);
        ULONGLONG startTime = GetTickCount64();

        // 等待服务停止
        while (QueryServiceStatus(service, &status))
        {
            if (status.dwCurrentState == SERVICE_STOPPED)
            {
                result = TRUE;
                break;
            }
            if (GetTickCount64() - startTime > timeout)
            {
                result = FALSE;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        CloseServiceHandle(service);
        CloseServiceHandle(scm);
        return result;
    }

    BOOL DeleteDriverService(const std::wstring& serviceName)
    {
        // 先尝试停止服务
        StopDriver(serviceName);

        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (!scm)
        {
            return FALSE;
        }

        SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), DELETE);
        if (!service)
        {
            CloseServiceHandle(scm);
            return FALSE;
        }

        BOOL result = DeleteService(service);
        DWORD lastError = GetLastError();

        CloseServiceHandle(service);
        CloseServiceHandle(scm);

        // 处理服务不存在的情况
        return result || (lastError == ERROR_SERVICE_DOES_NOT_EXIST);
    }

private:
    WatchDog() = default;
    WatchDog(const WatchDog&) = delete;
    WatchDog& operator=(const WatchDog&) = delete;

    PROCESS_INFORMATION pi = { 0 };
};