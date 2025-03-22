#include "wKFilters.h"
#include "wKDatabase.h"

PFLT_FILTER gFilterHandle = NULL;

const FLT_OPERATION_REGISTRATION Callbacks[] = 
{
    { IRP_MJ_CREATE, 0, PreCreateCallback, NULL },
    { IRP_MJ_SET_INFORMATION, 0, PreSetInformationCallback, NULL },
    { IRP_MJ_WRITE, 0, PreWriteCallback, NULL },
    { IRP_MJ_OPERATION_END }
};

const FLT_REGISTRATION FilterRegistration = 
{
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    UnloadCallback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) 
{
    UNREFERENCED_PARAMETER(FltObjects);

    //if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_CREATE)) 
    {
        PFLT_FILE_NAME_INFORMATION fileNameInfo;
        NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo);
        if (NT_SUCCESS(status)) 
        {
            FltParseFileNameInformation(fileNameInfo);
            if (Database::GetInstance().Lock<check_l>(fileNameInfo->Name.Buffer, fileNameInfo->Name.Length) == R_FIND_PATH)
            {
                DbgPrint("[CREATE] File: %wZ\n", &fileNameInfo->Name);
            }
            FltReleaseFileNameInformation(fileNameInfo);
        }
    }
    CompletionContext = NULL;
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS PreSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) 
{
    UNREFERENCED_PARAMETER(FltObjects);

    if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformation) 
    {
        PFILE_DISPOSITION_INFO info = (PFILE_DISPOSITION_INFO)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        if (info->DeleteFile) 
        {
            PFLT_FILE_NAME_INFORMATION fileNameInfo;
            NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo);
            if (NT_SUCCESS(status)) 
            {
                FltParseFileNameInformation(fileNameInfo);
                if (Database::GetInstance().Lock<check_l>(fileNameInfo->Name.Buffer, fileNameInfo->Name.Length) == R_FIND_PATH)
                {
                    DbgPrint("[DELETE] File: %wZ\n", &fileNameInfo->Name);
                }
                FltReleaseFileNameInformation(fileNameInfo);
            }
        }
    }
    CompletionContext = NULL;
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS PreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) 
{
    UNREFERENCED_PARAMETER(FltObjects);

    ULONG length = Data->Iopb->Parameters.Write.Length;
    LONGLONG offset = Data->Iopb->Parameters.Write.ByteOffset.QuadPart;

    //PMDL mdl = NULL;
    PVOID buffer = NULL;

    if (Data->Iopb->Parameters.Write.MdlAddress == NULL) 
    {
        NTSTATUS s = FltLockUserBuffer(Data);
        if (!NT_SUCCESS(s)) {
            goto exit;
        }
    }
    buffer = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Write.MdlAddress, NormalPagePriority);

    if (buffer) {
        PFLT_FILE_NAME_INFORMATION nameInfo;
        NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &nameInfo);
        if (NT_SUCCESS(status)) {
            FltParseFileNameInformation(nameInfo);
            if (Database::GetInstance().Lock<check_l>(nameInfo->Name.Buffer, nameInfo->Name.Length) == R_FIND_PATH)
            {
                DbgPrint("[WRITE] File: %wZ, Offset: %lld, Length: %lu\n", &nameInfo->Name, offset, length);
            }
            FltReleaseFileNameInformation(nameInfo);
        }

        // 打印前 16 字节（示例）
        //DbgPrint("Data: ");
        //for (ULONG i = 0; i < min(16, length); i++) {
        //    DbgPrint("%02X ", ((PUCHAR)buffer)[i]);
        //}
        //DbgPrint("\n");
    }


exit:
    CompletionContext = NULL;
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

NTSTATUS UnloadCallback(_In_ FLT_FILTER_UNLOAD_FLAGS Flags) 
{
    UNREFERENCED_PARAMETER(Flags);
    FltUnregisterFilter(gFilterHandle);
    return STATUS_SUCCESS;
}
