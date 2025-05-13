#include "wKKComm.h"
#include "wKDatabase.h"

KCommunication KCommunication::instance;

NTSTATUS KCommunication::Init()
{
    NTSTATUS status;
    UNICODE_STRING portName = RTL_CONSTANT_STRING(L"\\SFKCommMFPort");
    OBJECT_ATTRIBUTES portOA;
    PSECURITY_DESCRIPTOR sd;

    status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("[FATAL] FltBuildDefaultSecurityDescriptor %X", status);
        return status;
    }

    InitializeObjectAttributes(&portOA, &portName, OBJ_KERNEL_HANDLE, NULL, sd);

    status = FltCreateCommunicationPort(
        gFilterHandle,
        &MFCommServerPort,
        &portOA,
        NULL,
        ConnectNotifyCallback,
        DisconnectNotifyCallback,
        MessageNotifyCallback,
        2                       //允许两个用户态的管理程序连接本内核态程序
    );
    return status;
}

void KCommunication::Close()
{
	if (MFCommServerPort != NULL)
	{
		FltCloseCommunicationPort(MFCommServerPort);
	}
}

//发给Cli
sfresult KCommunication::SendKCommMessage(PMessage message)
{
    if (!MFCommServerPort) return R_NO_CONNECTION;
    if (!CClientPort) return R_NO_CONNECTION;
    PMessage reply = (PMessage)ExAllocatePoolUninitialized(PagedPool, sizeof(Message), 'rMsg');
    if (!reply) return R_BAD_ALLOC;
    RtlZeroMemory(reply, sizeof(Message));

    ULONG lreply = sizeof(Message);
    LARGE_INTEGER timeout = { 0 };
    timeout.QuadPart = -20'000'000; //2秒后超时

    NTSTATUS s = FltSendMessage(gFilterHandle, &CClientPort, message, (ULONG)message->size, reply, &lreply, &timeout);
    if (s != STATUS_SUCCESS)
    {
        ExFreePoolWithTag(reply, 'rMsg');
        switch (s)
        {
        case STATUS_INSUFFICIENT_RESOURCES:
            return R_BAD_ALLOC;
        case STATUS_PORT_DISCONNECTED:
        case STATUS_THREAD_IS_TERMINATING:
        case STATUS_TIMEOUT:
            return R_NO_CONNECTION;
        }
        return R_UNSUCCESS;
    }

    sfresult r = reply->ResultAndError.result;
    ExFreePoolWithTag(reply, 'rMsg');
    return r;
}

NTSTATUS
KCommunication::MessageNotifyCallback(
    __in_opt PVOID PortCookie,
    __in_opt PVOID InputBuffer,
    __in ULONG InputBufferLength,
    __out_opt PVOID OutputBuffer,
    __in ULONG OutputBufferLength,
    __out PULONG ReturnOutputBufferLength
)
{
    UNREFERENCED_PARAMETER(PortCookie);

    //__debugbreak();

    __try
    {
        ProbeForRead(InputBuffer, InputBufferLength, sizeof(ULONG));
        if (InputBufferLength < sizeof(Message) || InputBuffer == NULL || OutputBufferLength < sizeof(Message) || OutputBuffer == NULL)
        {
            if (OutputBufferLength >= sizeof(Message) && OutputBuffer != NULL)
            {
                ProbeForWrite(OutputBuffer, OutputBufferLength, sizeof(ULONG));
                ((PMessage)OutputBuffer)->size = sizeof(Message);
                ((PMessage)OutputBuffer)->type = M_RESULT;
                ((PMessage)OutputBuffer)->ResultAndError.result = R_INVALID_LENGTH;
                *ReturnOutputBufferLength = sizeof(Message);
                return STATUS_SUCCESS;
            }
            else
            {
                *ReturnOutputBufferLength = 0;
                return STATUS_SUCCESS;
            }
        }
        PMessage buffer = (PMessage)ExAllocatePoolUninitialized(NonPagedPool, InputBufferLength, 'bMsg');
        if (buffer == NULL)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(buffer, InputBuffer, InputBufferLength);
        sfresult r = R_OK;

        switch (buffer->type)
        {
        case M_RESULT:
            r = R_OK;
            break;

        case M_ADD_PATH:
            r = Database::GetInstance().Lock<add_l>(buffer->AddPath.path,
                buffer->AddPath.length,
                buffer->AddPath.hash);
            break;

        case M_DEL_PATH:
            r = Database::GetInstance().Lock<del_l>(buffer->DelPath.hash);
            break;

        case M_QUERY_PATH:
            r = Database::GetInstance().Lock<isIn_l>(buffer->QueryPath.hash);
            break;
        case M_FILE_CREATE: //因为实际上删除文件等操作也是用create实现的，所以还是得处理这个
        {
            //__debugbreak();

            HANDLE hFile = NULL;
            PFILE_OBJECT FileObj = NULL;
            OBJECT_ATTRIBUTES objAttr = { 0 };
            UNICODE_STRING path = { (USHORT)(buffer->Create.EaOffset - 2), (USHORT)buffer->Create.EaOffset , (WCHAR*)(buffer->Create.PathAndEaBuffer) };
            InitializeObjectAttributes(&objAttr, &path, OBJ_OPENIF | OBJ_KERNEL_HANDLE, NULL, NULL);
            IO_STATUS_BLOCK ioStatus = { 0 };

            FltCreateFileEx(gFilterHandle,
                gFilterInstance,
                &hFile,
                &FileObj,
                buffer->Create.DesiredAccess,
                &objAttr,
                &ioStatus,
                &buffer->Create.AllocationSize,
                buffer->Create.FileAttributes,
                buffer->Create.ShareAccess,
                buffer->Create.CreateDisposition,
                buffer->Create.CreateOptions,
                (buffer->Create.PathAndEaBuffer + buffer->Create.EaOffset),
                buffer->Create.EaLength,
                NULL
            );

            ObDereferenceObject(FileObj);
            FltClose(hFile);

            break;
        }
        case M_FILE_WRITE:
        {
            //__debugbreak();

            HANDLE hFile = NULL;
            PFILE_OBJECT FileObj = NULL;
            OBJECT_ATTRIBUTES objAttr = { 0 };
            UNICODE_STRING path = { (USHORT)(buffer->Write.WriteOffset - 2), (USHORT)buffer->Write.WriteOffset , (WCHAR*)(buffer->Write.PathAndWriteBuffer) };
            InitializeObjectAttributes(&objAttr, &path, OBJ_OPENIF | OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
            IO_STATUS_BLOCK ioStatus = { 0 };

            FltCreateFileEx(gFilterHandle,
                gFilterInstance,
                &hFile,
                &FileObj,
                GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                &objAttr,
                &ioStatus,
                0,
                FILE_ATTRIBUTE_NORMAL,
                0,
                FILE_OPEN_IF,
                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_COMPLETE_IF_OPLOCKED,
                NULL,
                0,
                NULL
            );

            ULONG hasWritten = 0;
            FltWriteFile(gFilterInstance,
                FileObj,
                &buffer->Write.ByteOffset,
                buffer->Write.Length,
                (PVOID)(buffer->Write.PathAndWriteBuffer+buffer->Write.WriteOffset),
                FLTFL_IO_OPERATION_NON_CACHED,
                &hasWritten,
                NULL,
                NULL
            );

            ObDereferenceObject(FileObj);
            FltClose(hFile);

            break;
        }

        default:
            r = R_ERROR_FUNC;
        }
        ExFreePoolWithTag(buffer, 'bMsg');

        ProbeForWrite(OutputBuffer, OutputBufferLength, sizeof(ULONG));
        ((PMessage)OutputBuffer)->size = sizeof(Message);
        ((PMessage)OutputBuffer)->type = M_RESULT;
        ((PMessage)OutputBuffer)->ResultAndError.result = r;
        *ReturnOutputBufferLength = sizeof(Message);
    } __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("[ERROR] in MessageNotifyCallback: %lu\n", GetExceptionCode());
        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}

VOID
KCommunication::DisconnectNotifyCallback(
    __in_opt PVOID ConnectionCookie
)
{
    PCLIENT_PORT_CONTEXT context = (PCLIENT_PORT_CONTEXT)ConnectionCookie;
    if (context != NULL)
    {
        if (context->ClientPort == GetInstance().CClientPort)
        {
            GetInstance().CClientPort = NULL;
        }
        else if (context->ClientPort == GetInstance().GClientPort)
        {
            GetInstance().GClientPort = NULL;
        }
        FltCloseClientPort(gFilterHandle, &(context->ClientPort));
        ExFreePoolWithTag(context, 'hCli');
    }
}

NTSTATUS
KCommunication::ConnectNotifyCallback(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __out PVOID* ConnectionCookie
)
{
    UNREFERENCED_PARAMETER(ServerPortCookie);

    if (SizeOfContext > 0)
    {
        if (*(char*)ConnectionContext == 'C')
        {
            GetInstance().CClientPort = ClientPort;
        }
        else if (*(char*)ConnectionContext == 'G')
        {
            GetInstance().GClientPort = ClientPort;
        }
        else
        {
            return STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    PCLIENT_PORT_CONTEXT context;

    context = (PCLIENT_PORT_CONTEXT)ExAllocatePoolUninitialized(PagedPool, sizeof(CLIENT_PORT_CONTEXT), 'hCli'); //这个回调的IRQL是PASSIVE_LEVEL，因此用pagedpool内存，省一些操作系统资源
    if (context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    context->ClientPort = ClientPort;
    *ConnectionCookie = context;

    

    return STATUS_SUCCESS;
}
