#include "wUCComm.h"
#include "wUNetwork.h"
#include "USFMessage.h"
#include "USFLog.h"

int UCommunication::Start()
{
    HRESULT hr;

    Log("���ӵ������˿�");
    hr = FilterConnectCommunicationPort(
        L"\\SFKCommMFPort",
        0,
        "C",
        2,
        NULL,
        &hPort
    );

    if (FAILED(hr)) {
        Log("����ʧ�� ������: 0x%08X", hr);
        return 1;
    }

    Log("����mf��Ϣ�����߳�");
    messageThread = std::thread(&UCommunication::MessageThreadProc, this);

    return 0;
}

void UCommunication::MessageThreadProc()
{
    BYTE buffer[8192];
    PMessage pMsg = (PMessage)(buffer + sizeof(FILTER_MESSAGE_HEADER));
    HANDLE completion = CreateIoCompletionPort(hPort, NULL, 0, 1);
    LPOVERLAPPED pOvlp;
    DWORD outSize;
    ULONG_PTR key;

    while (TRUE)
    {
        //Log("ѭ���߳�");
        OVERLAPPED Ovlp;

        HRESULT hr = FilterGetMessage(
            hPort,
            (PFILTER_MESSAGE_HEADER)buffer,
            sizeof(buffer),
            &Ovlp
        );

        hr = GetQueuedCompletionStatus(completion, &outSize, &key, &pOvlp, INFINITE);

        if (FAILED(hr)) {
            Log("GetQueuedCompletionStatusʧ��: 0x%08X", hr);
            CloseHandle(hPort);
            break;
        }

        Log("����ͬ���͵���Ϣ");
        switch (pMsg->type) {
        case M_HEART_BEAT:
            Log("[����] ���к�: %zu", pMsg->HeartBeat.seq);
            break;

        case M_RESULT:
            Log("[�������] ״̬��: %d", pMsg->ResultAndError.result);
            break;

        case M_KERNEL_ERROR:
            Log("[�ں˴���] ������: %d", pMsg->ResultAndError.result);
            break;

        case M_FILE_CREATE:
        case M_FILE_WRITE:
        case M_FILE_DEL:
        {
            int r = UNetwork::GetInstance().Broadcast(pMsg);
            if (SOCKET_ERROR == r)
            {
                Log("Broadcast error: %X", WSAGetLastError());
            }
            break;
        }

        default:
            Log("�յ�δ֪������Ϣ: %lld", pMsg->type);
            break;
        }

        // ���ͻظ��������Ҫ��
        PFILTER_REPLY_HEADER reply = (PFILTER_REPLY_HEADER)buffer;
        reply->MessageId = ((PFILTER_MESSAGE_HEADER)buffer)->MessageId;
        reply->Status = 0;

        hr = FilterReplyMessage(
            hPort,
            reply,
            sizeof(FILTER_MESSAGE_HEADER) + sizeof(Message)  // ����ʵ����Ҫ���ظ�����
        );

        if (FAILED(hr)) {
            Log("�ظ���Ϣʧ��: 0x%08X", hr);
        }
    }

    return;
}

int UCommunication::SendUCommMessage(PMessage msg)
{
    if (hPort == NULL) return 1;

    CHAR replyBuffer[sizeof(Message)] = { 0 };
    DWORD replyLength = 0;

    HRESULT hr = FilterSendMessage(
        hPort,
        msg,
        (DWORD)msg->size,
        replyBuffer,
        sizeof(replyBuffer),
        &replyLength
    );
    if (FAILED(hr)) {
        Log("������Ϣʧ��: 0x%08X", hr);
        CloseHandle(hPort);
        return 1;
    }
    if (((PMessage)replyBuffer)->ResultAndError.result == R_OK)
    {
        return 0;
    }
    else
    {
        Log("Reply from wK error: 0x%X", ((PMessage)replyBuffer)->ResultAndError.result);
        return 1;
    }
}