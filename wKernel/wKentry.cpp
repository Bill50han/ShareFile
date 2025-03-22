#include "wKFilters.h"
#include "wKKComm.h"
#define NOINF

#ifdef NOINF
#define MAX_PATH 260
NTSTATUS PrepMiniFilter(IN PUNICODE_STRING reg_path, IN PWSTR altiude) 
{
    /*
    minifilter��ܵ�������������ע�����������ֵ������������ں�����������inf�ļ�������
    ����Ϊ��û��Ǯ�����ǩ����֤�飬�������������մ���ʲ���ͨ��inf������;�����ء�
    ��ʱ��Ҫ����ʹ��minifilter��ܣ������ֶ�������Щ�����ֵ��
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
    PrepMiniFilter(RegistryPath, L"136650.5");   //����һ���ҵ���һ����Ŀ��΢�����������minifilter���α��
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