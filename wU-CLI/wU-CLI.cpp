#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fltUser.h>


DWORD WINAPI MessageThread(LPVOID lpParam);

int main()
{
    HANDLE hPort = NULL;
    HRESULT hr;

    // 连接到驱动端口
    hr = FilterConnectCommunicationPort(
        L"\\MyFilterPort",
        0,
        "C",
        2,
        NULL,
        &hPort
    );

    if (FAILED(hr)) {
        printf("Connect failed: 0x%x\n", hr);
        return 1;
    }

    // 启动消息接收线程
    HANDLE hThread = CreateThread(NULL, 0, MessageThread, hPort, 0, NULL);
    if (hThread == NULL) {
        printf("CreateThread failed: %d\n", GetLastError());
        CloseHandle(hPort);
        return 1;
    }

    // 发送消息到驱动
    CHAR message[] = "Hello from User Mode!";
    CHAR replyBuffer[100];
    DWORD replyLength;

    hr = FilterSendMessage(
        hPort,
        message,
        sizeof(message),
        replyBuffer,
        sizeof(replyBuffer),
        &replyLength
    );

    if (SUCCEEDED(hr)) {
        printf("Received reply: %s\n", replyBuffer);
    }
    else {
        printf("FilterSendMessage failed: 0x%x\n", hr);
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hPort);
    return 0;
}

DWORD WINAPI MessageThread(LPVOID lpParam)
{
    HANDLE hPort = (HANDLE)lpParam;
    BYTE buffer[1024];
    PFILTER_MESSAGE_HEADER msg = (PFILTER_MESSAGE_HEADER)buffer;

    while (TRUE)
    {
        HRESULT hr = FilterGetMessage(
            hPort,
            msg,
            sizeof(buffer),
            NULL
        );

        if (FAILED(hr)) {
            printf("FilterGetMessage failed: 0x%x\n", hr);
            break;
        }

        // 处理接收到的消息
        printf("Received from kernel: %s\n", (char*)(msg + 1));

        // 发送回复
        PFILTER_MESSAGE_HEADER reply = (PFILTER_MESSAGE_HEADER)buffer;
        reply->MessageId = msg->MessageId;
        strcpy_s((char*)(reply + 1), sizeof(buffer) - sizeof(FILTER_MESSAGE_HEADER), "Ack");

        DWORD replySize = sizeof(FILTER_MESSAGE_HEADER) + (DWORD)strlen("Ack") + 1;
        hr = FilterReplyMessage(
            hPort,
            reply,
            replySize
        );

        if (FAILED(hr)) {
            printf("FilterReplyMessage failed: 0x%x\n", hr);
        }
    }

    return 0;
}