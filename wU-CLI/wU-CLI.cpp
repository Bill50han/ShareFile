#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <cstring>
#include <clocale>
#include <cwchar>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fltUser.h>

enum sfresult
{
    R_OK,
    R_FIND_PATH = R_OK,

    R_UNSUCCESS,

    R_BAD_ALLOC,
    R_NOT_FIND_PATH,
    R_ERROR_FUNC,

    R_INVALID_LENGTH,

    R_NO_CONNECTION
};
using sferror = sfresult;

enum MessageType : LONG64
{
    M_HEART_BEAT,	//心跳包
    M_RESULT,		//操作的返回值

    M_ADD_PATH,		//对应Database::add
    M_DEL_PATH,		//对应Database::del
    M_QUERY_PATH,	//对应Database::isIn

    M_FILE_CREATE,	//对应filter create回调
    M_FILE_WRITE,	//对应filter write回调
    M_FILE_DEL,		//对应filter setinfo回调里的del情况

    M_KERNEL_ERROR,	//内核程序主动发生错误
};

typedef struct _Message
{
    size_t size;
    MessageType type;

    union
    {
        struct
        {
            size_t seq;
        } HeartBeat;

        struct
        {
            sfresult result;
        } ResultAndError;

        struct
        {
            size_t length;
            size_t hash;
            wchar_t path[1];
        } AddPath;

        struct
        {
            size_t hash;
        } DelPath;

        struct
        {
            size_t hash;
        } QueryPath;

        struct	//以下两个结构体内容直接对应windows api ZwCreateFile与ZwWriteFile的参数，因此采用微软的变量命名习惯
        {
            ACCESS_MASK DesiredAccess;
            LARGE_INTEGER AllocationSize;
            USHORT FileAttributes;
            USHORT ShareAccess;
            ULONG CreateDisposition;
            ULONG CreateOptions;
            ULONG EaLength;

            size_t EaOffset;

            unsigned char PathAndEaBuffer[1];	//这里只是把unsigned char当byte用，path是wchar，ea中也可能有0。
            //因此整体包大小由最外层的size或本层的EaOffset和EaLength双保险共同确定。
        } Create;

        struct
        {
            ULONG Length;
            ULONG Key;
            LARGE_INTEGER ByteOffset;

            size_t WriteOffset;

            unsigned char PathAndWriteBuffer[1];	//同上
        } Write;

        struct
        {
            size_t length;
            wchar_t path[1]; //这个path的类型是wchar[]！
        } Delete;
    };
} Message, * PMessage;

DWORD WINAPI MessageThreadProc(LPVOID lpParam);

HANDLE hPort = NULL;
int main()
{
    setlocale(LC_ALL, "");
    HRESULT hr;

    printf("连接到驱动端口\n");
    hr = FilterConnectCommunicationPort(
        L"\\SFKCommMFPort",
        0,
        "C",
        2,
        NULL,
        &hPort
    );

    if (FAILED(hr)) {
        printf("连接失败 错误码: 0x%08X\n", hr);
        return 1;
    }

    printf("创建消息接收线程\n");
    HANDLE hThread = CreateThread(
        NULL,
        0,
        MessageThreadProc,
        hPort,
        0,
        NULL
    );

    if (hThread == NULL) {
        printf("创建接收线程失败: %d\n", GetLastError());
        CloseHandle(hPort);
        return 1;
    }

    printf("构造 AddPath 消息\n");
    const wchar_t* targetPath = L"\\Device\\HarddiskVolume1\\test.txt";
    const size_t pathLen = wcslen(targetPath) + 1;
    const size_t msgSize = sizeof(Message) + pathLen * sizeof(wchar_t);

    PMessage pMsg = (PMessage)malloc(msgSize);
    if (!pMsg) {
        printf("内存分配失败\n");
        CloseHandle(hPort);
        return 1;
    }

    // 填充消息内容
    ZeroMemory(pMsg, msgSize);
    pMsg->size = msgSize;
    pMsg->type = M_ADD_PATH;
    pMsg->AddPath.length = (pathLen-1) * sizeof(wchar_t);
    pMsg->AddPath.hash = 111; // 根据实际需求计算哈希值

    // 拷贝路径数据
    wcscpy(
        pMsg->AddPath.path,
        targetPath);

    printf("发送 AddPath 请求\n");
    CHAR replyBuffer[1024];
    DWORD replyLength = 0;

    hr = FilterSendMessage(
        hPort,
        pMsg,
        (DWORD)pMsg->size,
        replyBuffer,
        sizeof(replyBuffer),
        &replyLength
    );

    free(pMsg);

    if (FAILED(hr)) {
        printf("发送消息失败: 0x%08X\n", hr);
        CloseHandle(hPort);
        return 1;
    }

    printf("Reply: %X\n", ((PMessage)replyBuffer)->ResultAndError.result);

    // 5. 等待接收线程结束
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hPort);
    return 0;
}

// 消息接收线程处理函数
DWORD WINAPI MessageThreadProc(LPVOID lpParameter)
{
    HANDLE hPort = (HANDLE)lpParameter;
    BYTE buffer[8192];
    PMessage pMsg = (PMessage)(buffer+sizeof(FILTER_MESSAGE_HEADER));
    HANDLE completion = CreateIoCompletionPort(hPort, NULL, 0, 1);
    LPOVERLAPPED pOvlp;
    BOOL result;
    DWORD outSize;
    ULONG_PTR key;

    while (TRUE)
    {
        printf("循环线程\n");
        OVERLAPPED Ovlp;

        HRESULT hr = FilterGetMessage(
            hPort,
            (PFILTER_MESSAGE_HEADER)buffer,
            sizeof(buffer),
            &Ovlp
        );
        //if (FAILED(hr)) {
        //    printf("接收消息失败: 0x%08X\n", hr);
        //    break;
        //}

        hr= GetQueuedCompletionStatus(completion, &outSize, &key, &pOvlp, INFINITE);

        if (FAILED(hr)) {
            printf("GetQueuedCompletionStatus失败: 0x%08X\n", hr);
            CloseHandle(hPort);
            break;
        }

        printf("处理不同类型的消息\n");
        switch (pMsg->type) {
        case M_HEART_BEAT:
            printf("[心跳] 序列号: %zu\n", pMsg->HeartBeat.seq);
            break;

        case M_RESULT:
            printf("[操作结果] 状态码: %d\n", pMsg->ResultAndError.result);
            break;

        case M_KERNEL_ERROR:
            printf("[内核错误] 错误码: %d\n", pMsg->ResultAndError.result);
            break;

        case M_FILE_CREATE:
            printf("[Create] ");
            wprintf(L"%ls\n", pMsg->Create.PathAndEaBuffer);
            break;
        case M_FILE_WRITE:
            printf("[Write] ");
            wprintf(L"%ls : ", pMsg->Write.PathAndWriteBuffer);
            printf("%s\n", pMsg->Write.PathAndWriteBuffer + pMsg->Write.WriteOffset);
            break;
        case M_FILE_DEL:
            // 根据实际需求处理文件操作通知
            break;

        default:
            printf("收到未知类型消息: %lld\n", pMsg->type);
            break;
        }

        // 发送回复（如果需要）
        PFILTER_REPLY_HEADER reply = (PFILTER_REPLY_HEADER)buffer;
        reply->MessageId = ((PFILTER_MESSAGE_HEADER)buffer)->MessageId;
        reply->Status = 0;

        hr = FilterReplyMessage(
            hPort,
            reply,
            sizeof(FILTER_MESSAGE_HEADER)+sizeof(Message)  // 根据实际需要填充回复内容
        );

        if (FAILED(hr)) {
            printf("回复消息失败: 0x%08X\n", hr);
        }
    }

    return 0;
}