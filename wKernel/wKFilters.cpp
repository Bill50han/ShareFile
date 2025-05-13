#include "wKFilters.h"
#include "wKDatabase.h"
#include "wKKComm.h"

PFLT_FILTER gFilterHandle = NULL;
PFLT_INSTANCE gFilterInstance = NULL;

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
    SetupInstance,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

NTSTATUS SetupInstance(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
    gFilterInstance = FltObjects->Instance;
    return STATUS_SUCCESS;
}

FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
) 
{

    //if (FlagOn(Data->Iopb->Parameters.Create.Options, FILE_CREATE)) 
    {
        PFLT_FILE_NAME_INFORMATION fileNameInfo;
        NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &fileNameInfo);
        if (NT_SUCCESS(status)) 
        {
            FltParseFileNameInformation(fileNameInfo);
            if (Database::GetInstance().Lock<check_l>(fileNameInfo->Name.Buffer, fileNameInfo->Name.Length) == R_FIND_PATH)
            {
                DbgPrint("[CREATE] File: %wZ IRP Flags: 0x%x IRQL: %x Create Flags: %x\n", &fileNameInfo->Name, Data->Iopb->IrpFlags, KeGetCurrentIrql(), Data->Iopb->Parameters.Create.Options);

                //__try
                {
                    PMessage msg = (PMessage)FltAllocatePoolAlignedWithTag(FltObjects->Instance, NonPagedPool, sizeof(Message) + fileNameInfo->Name.Length + sizeof(L'\0') + Data->Iopb->Parameters.Create.EaLength + sizeof(L'\0'), 'sMsg');
                    if (msg == NULL)
                    {
                        DbgPrint("[FATAL] No Memory\n");
                        CompletionContext = NULL;
                        return FLT_PREOP_SUCCESS_NO_CALLBACK;
                    }
                    msg->size = sizeof(Message) + fileNameInfo->Name.Length + sizeof(L'\0') + Data->Iopb->Parameters.Create.EaLength + sizeof(L'\0');
                    msg->type = M_FILE_CREATE;

                    msg->Create.DesiredAccess = Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
                    msg->Create.AllocationSize.QuadPart = Data->Iopb->Parameters.Create.AllocationSize.QuadPart;
                    msg->Create.FileAttributes = Data->Iopb->Parameters.Create.FileAttributes;
                    msg->Create.ShareAccess = Data->Iopb->Parameters.Create.ShareAccess;
                    msg->Create.CreateOptions = Data->Iopb->Parameters.Create.Options & 0xFF'FFFF;
                    msg->Create.CreateDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0xFF;
                    msg->Create.EaLength = Data->Iopb->Parameters.Create.EaLength;

                    memcpy(msg->Create.PathAndEaBuffer, fileNameInfo->Name.Buffer, fileNameInfo->Name.Length);
                    msg->Create.PathAndEaBuffer[fileNameInfo->Name.Length] = 0;
                    msg->Create.PathAndEaBuffer[fileNameInfo->Name.Length + 1] = 0;

                    msg->Create.EaOffset = fileNameInfo->Name.Length + sizeof(L'\0');
                    memcpy(msg->Create.PathAndEaBuffer + fileNameInfo->Name.Length + sizeof(L'\0'), Data->Iopb->Parameters.Create.EaBuffer, Data->Iopb->Parameters.Create.EaLength);

                    KCommunication::GetInstance().KCommunication::SendKCommMessage(msg);

                    FltFreePoolAlignedWithTag(FltObjects->Instance, msg, 'sMsg');
                }
                //__except (EXCEPTION_EXECUTE_HANDLER)
                //{
                    //DbgPrint("[ERROR] in create: %lu\n", GetExceptionCode());
                //}
            }
            __try
            {
                FltReleaseFileNameInformation(fileNameInfo);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                DbgPrint("[ERROR] in create.FltReleaseFileName: %lu\n", GetExceptionCode());
                CompletionContext = NULL;
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
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

                    PMessage msg = (PMessage)ExAllocatePoolUninitialized(NonPagedPool, sizeof(Message) + fileNameInfo->Name.Length + sizeof(L'\0'), 'sMsg');
                    if (msg == NULL)
                    {
                        DbgPrint("[FATAL] No Memory\n");
                        CompletionContext = NULL;
                        return FLT_PREOP_SUCCESS_NO_CALLBACK;
                    }
                    msg->size = sizeof(Message) + fileNameInfo->Name.Length + sizeof(L'\0');
                    msg->type = M_FILE_DEL;

                    msg->Delete.length = fileNameInfo->Name.Length;
                    memcpy(msg->Delete.path, fileNameInfo->Name.Buffer, fileNameInfo->Name.Length);
                    msg->Delete.path[fileNameInfo->Name.Length >> 1] = L'\0';

                    KCommunication::GetInstance().KCommunication::SendKCommMessage(msg);

                    ExFreePoolWithTag(msg, 'sMsg');
                }
                __try
                {
                    FltReleaseFileNameInformation(fileNameInfo);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    DbgPrint("[ERROR] in del.FltReleaseFileName: %lu\n", GetExceptionCode());
                    CompletionContext = NULL;
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
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

    if (FlagOn(Data->Iopb->IrpFlags, IRP_PAGING_IO))
    {
        goto exit;
    }

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
        if (NT_SUCCESS(status)) 
        {
            FltParseFileNameInformation(nameInfo);
            if (Database::GetInstance().Lock<check_l>(nameInfo->Name.Buffer, nameInfo->Name.Length) == R_FIND_PATH)
            {
                DbgPrint("[WRITE] File: %wZ, Offset: %lld, Length: %lu IRP Flags: 0x%x\n", &nameInfo->Name, offset, length, Data->Iopb->IrpFlags);

                PMessage msg = (PMessage)ExAllocatePoolUninitialized(NonPagedPool, sizeof(Message) + nameInfo->Name.Length + sizeof(L'\0') + Data->Iopb->Parameters.Write.Length + sizeof(L'\0'), 'sMsg');
                if (msg == NULL)
                {
                    DbgPrint("[FATAL] No Memory\n");
                    CompletionContext = NULL;
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
                msg->size = sizeof(Message) + nameInfo->Name.Length + sizeof(L'\0') + Data->Iopb->Parameters.Write.Length + sizeof(L'\0');
                msg->type = M_FILE_WRITE;

                msg->Write.Length = Data->Iopb->Parameters.Write.Length;
                msg->Write.Key = Data->Iopb->Parameters.Write.Key;
                msg->Write.ByteOffset.QuadPart = Data->Iopb->Parameters.Write.ByteOffset.QuadPart;

                memcpy(msg->Write.PathAndWriteBuffer, nameInfo->Name.Buffer, nameInfo->Name.Length);
                msg->Write.PathAndWriteBuffer[nameInfo->Name.Length] = 0;
                msg->Write.PathAndWriteBuffer[nameInfo->Name.Length + 1] = 0;

                msg->Write.WriteOffset = nameInfo->Name.Length + sizeof(L'\0');
                memcpy(msg->Write.PathAndWriteBuffer + nameInfo->Name.Length + sizeof(L'\0'), buffer, Data->Iopb->Parameters.Write.Length);
                msg->Write.PathAndWriteBuffer[msg->size - 1] = 0;
                msg->Write.PathAndWriteBuffer[msg->size - 2] = 0;

                DbgPrint("%p %p\n", buffer, msg);
                //__debugbreak();

                KCommunication::GetInstance().KCommunication::SendKCommMessage(msg);

                __try
                {
                    ExFreePoolWithTag(msg, 'sMsg');
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    DbgPrint("[ERROR] in write.ExFreePool: %lu\n", GetExceptionCode());
                    CompletionContext = NULL;
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }
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
    KCommunication::GetInstance().Close();
    return STATUS_SUCCESS;
}
