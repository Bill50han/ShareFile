#include "wKFilters.h"
#include "wKKComm.h"
#define NOINF

#ifdef NOINF
#define MAX_PATH 260
NTSTATUS PrepMiniFilter(IN PUNICODE_STRING reg_path, IN PWSTR altiude) 
{
    /*
    minifilter框架的正常工作依赖注册表里的特殊键值，正常情况由内核驱动附带的inf文件描述。
    但因为我没有钱买代码签名的证书，因此这个程序最终大概率不会通过inf的正常途径加载。
    此时想要正常使用minifilter框架，必须手动创建这些特殊键值。
    */
    BOOLEAN result = FALSE;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PWSTR driver_name = NULL;
    WCHAR key_path[MAX_PATH] = { 0 };
    WCHAR default_instance_value_data[MAX_PATH] = { 0 };
    if (reg_path == NULL) return result;
    if (reg_path->Buffer == NULL) return result;
    if (reg_path->Length <= 0) return result;
    if (altiude == NULL) return result;
    if (altiude[0] == L'\0') return result;

    do {
        driver_name = wcsrchr(reg_path->Buffer, L'\\');
        if (!MmIsAddressValid(driver_name)) break;

        RtlZeroMemory(key_path, MAX_PATH * sizeof(WCHAR));
        // swprintf(key_path, L"%ws\\Instances", driver_name);
        status = RtlStringCbPrintfW(key_path, sizeof(key_path), L"%ws\\Instances",
            driver_name);
        if (!NT_SUCCESS(status)) break;
        status = RtlCreateRegistryKey(RTL_REGISTRY_SERVICES, key_path);
        if (!NT_SUCCESS(status)) break;

        //  swprintf(Data, L"%ws Instance", &driver_name[1]);
        status = RtlStringCbPrintfW(default_instance_value_data,
            sizeof(default_instance_value_data),
            L"%ws Instance", &driver_name[1]);
        if (!NT_SUCCESS(status)) break;

        status = RtlWriteRegistryValue(
            RTL_REGISTRY_SERVICES, key_path, L"DefaultInstance", REG_SZ,
            default_instance_value_data,
            (ULONG)(wcslen(default_instance_value_data) * sizeof(WCHAR) + 2));

        if (!NT_SUCCESS(status)) break;
        RtlZeroMemory(key_path, MAX_PATH * sizeof(WCHAR));

        // swprintf(key_path, L"%ws\\Instances%ws Instance", driver_name,
        // driver_name);
        status = RtlStringCbPrintfW(key_path, sizeof(key_path),
            L"%ws\\Instances%ws Instance", driver_name,
            driver_name);
        if (!NT_SUCCESS(status)) break;

        status = RtlCreateRegistryKey(RTL_REGISTRY_SERVICES, key_path);
        if (!NT_SUCCESS(status)) break;

        status = RtlCreateRegistryKey(RTL_REGISTRY_SERVICES, key_path);
        if (!NT_SUCCESS(status)) break;

        status = RtlWriteRegistryValue(
            RTL_REGISTRY_SERVICES, key_path, L"Altitude", REG_SZ, altiude,
            (ULONG)(wcslen(altiude) * sizeof(WCHAR) + 2));
        if (!NT_SUCCESS(status)) break;
        ULONG dwData = 0;
        status = RtlWriteRegistryValue(RTL_REGISTRY_SERVICES, key_path, L"Flags",
            REG_DWORD, &dwData, 4);
        if (!NT_SUCCESS(status)) break;
        result = TRUE;
    } while (FALSE);

    return result;
}
#endif // NOINF

extern "C" NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) 
{
#ifdef NOINF
    PrepMiniFilter(RegistryPath, L"136650.5");   //借用一下我的另一个项目在微软那里申请的minifilter海拔编号
#elif
    UNREFERENCED_PARAMETER(RegistryPath);
#endif // NOINF
    
    NTSTATUS status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    status = KCommunication::GetInstance().Init();
    if (NT_SUCCESS(status)) 
    {
        status = FltStartFiltering(gFilterHandle);
        if (!NT_SUCCESS(status)) 
        {
            KCommunication::GetInstance().Close();
            FltUnregisterFilter(gFilterHandle);
        }
    }

    return status;
}