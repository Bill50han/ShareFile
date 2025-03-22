#include "wKKComm.h"
#include "wKDatabase.h"

KCommunication KCommunication::instance;

NTSTATUS KCommunication::Init()
{
    NTSTATUS status;
    UNICODE_STRING portName = RTL_CONSTANT_STRING(L"\\SFKCommMFPort");
    OBJECT_ATTRIBUTES portOA;

    InitializeObjectAttributes(&portOA, &portName, OBJ_KERNEL_HANDLE, NULL, NULL);

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

sfresult KCommunication::SendKCommMessage(PMessage message)
{

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

    //TODO: add try catch
    __try
    {
        if (InputBufferLength != sizeof(Message) || InputBuffer == NULL || OutputBufferLength != sizeof(Message) || OutputBuffer == NULL)
        {
            if (OutputBufferLength == sizeof(Message) && OutputBuffer != NULL)
            {
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
        sfresult r = R_OK;

        switch (((PMessage)InputBuffer)->type)
        {
        case M_RESULT:
            r = R_OK;
            break;

        case M_ADD_PATH:
            r = Database::GetInstance().Lock<add_l>(((PMessage)InputBuffer)->AddPath.path,
                ((PMessage)InputBuffer)->AddPath.length,
                ((PMessage)InputBuffer)->AddPath.hash);
            break;

        case M_DEL_PATH:
            r = Database::GetInstance().Lock<del_l>(((PMessage)InputBuffer)->DelPath.hash);
            break;

        case M_QUERY_PATH:
            r = Database::GetInstance().Lock<isIn_l>(((PMessage)InputBuffer)->DelPath.hash);
            break;

        default:
            r = R_ERROR_FUNC;
        }

        ((PMessage)OutputBuffer)->size = sizeof(Message);
        ((PMessage)OutputBuffer)->type = M_RESULT;
        ((PMessage)OutputBuffer)->ResultAndError.result = r;
        *ReturnOutputBufferLength = sizeof(Message);
    } __except (EXCEPTION_EXECUTE_HANDLER)
    {
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
    ExFreePoolWithTag(context, 'hCli');
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
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);

    PCLIENT_PORT_CONTEXT context;

    context = (PCLIENT_PORT_CONTEXT)ExAllocatePoolUninitialized(PagedPool, sizeof(CLIENT_PORT_CONTEXT), 'hCli'); //这个回调的IRQL是PASSIVE_LEVEL，因此用pagedpool内存，省一些操作系统资源
    if (context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    context->ClientPort = ClientPort;
    *ConnectionCookie = context;

    return STATUS_SUCCESS;
}
