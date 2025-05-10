#include "wUNetwork.h"
#include "wUCComm.h"
#include "USFLog.h"

Logger logger=Logger(false,true,"clog.txt");    //无终端，打印到文件

extern "C" __declspec(dllexport) volatile bool g_running = true;

void onExit();

int main()
{
    Log("Begin!");
    setlocale(LC_ALL, "");
    
    if (UCommunication::GetInstance().Start())
    {
        exit(1);
    }
    if (!UNetwork::GetInstance().Start())
    {
        Log("INVALID_SOCKET");
        exit(2);
    }

    atexit(onExit);

    while (g_running)
    {
        SwitchToThread();   //实际逻辑在另外两个线程的循环里，主线程只需交出时间片，但别让进程结束就行。
    }

    return 0;
}

void onExit()
{
    Log("End!");
    UCommunication::GetInstance().Stop();
    UNetwork::GetInstance().Stop();
}