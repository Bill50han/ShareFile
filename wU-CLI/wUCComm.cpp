#include "wUCComm.h"
#include "wUNetwork.h"
#include "USFMessage.h"
#include "USFLog.h"

int UCommunication::Start()
{
    HRESULT hr;

    Log("连接到驱动端口");
    hr = FilterConnectCommunicationPort(
        L"\\SFKCommMFPort",
        0,
        "C",
        2,
        NULL,
        &hPort
    );

    if (FAILED(hr)) {
        Log("连接失败 错误码: 0x%08X", hr);
        return 1;
    }

    Log("创建mf消息接收线程");
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
        //Log("循环线程");
        OVERLAPPED Ovlp;

        HRESULT hr = FilterGetMessage(
            hPort,
            (PFILTER_MESSAGE_HEADER)buffer,
            sizeof(buffer),
            &Ovlp
        );

        hr = GetQueuedCompletionStatus(completion, &outSize, &key, &pOvlp, INFINITE);

        if (FAILED(hr)) {
            Log("GetQueuedCompletionStatus失败: 0x%08X", hr);
            CloseHandle(hPort);
            break;
        }

        Log("处理不同类型的消息");
        switch (pMsg->type) {
        case M_HEART_BEAT:
            Log("[心跳] 序列号: %zu", pMsg->HeartBeat.seq);
            break;

        case M_RESULT:
            Log("[操作结果] 状态码: %d", pMsg->ResultAndError.result);
            break;

        case M_KERNEL_ERROR:
            Log("[内核错误] 错误码: %d", pMsg->ResultAndError.result);
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
            Log("收到未知类型消息: %lld", pMsg->type);
            break;
        }

        // 发送回复（如果需要）
        PFILTER_REPLY_HEADER reply = (PFILTER_REPLY_HEADER)buffer;
        reply->MessageId = ((PFILTER_MESSAGE_HEADER)buffer)->MessageId;
        reply->Status = 0;

        hr = FilterReplyMessage(
            hPort,
            reply,
            sizeof(FILTER_MESSAGE_HEADER) + sizeof(Message)  // 根据实际需要填充回复内容
        );

        if (FAILED(hr)) {
            Log("回复消息失败: 0x%08X", hr);
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
        Log("发送消息失败: 0x%08X", hr);
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