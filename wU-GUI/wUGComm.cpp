#include "wUGComm.h"
#include "../wU-CLI/USFLog.h"

UCommunication UCommunication::instance;

int UCommunication::Start()
{
    HRESULT hr;

    Log("连接到驱动端口");
    hr = FilterConnectCommunicationPort(
        L"\\SFKCommMFPort",
        0,
        "G",
        2,
        NULL,
        &hPort
    );

    if (FAILED(hr)) {
        Log("连接失败 错误码: 0x%08X", hr);
        return 1;
    }

    return 0;
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