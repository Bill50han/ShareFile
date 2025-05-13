#include "wUNetwork.h"
#include "wUCComm.h"
#include "USFLog.h"

Logger logger=Logger(false,true,"clog.txt");    //无终端，打印到文件

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

    HANDLE hEvent = CreateEventW(NULL, FALSE, FALSE, L"wu-cli-should-close");

    WaitForSingleObject(hEvent, INFINITE);  //等待GUI给予关闭信号

    CloseHandle(hEvent);

    return 0;
}

void onExit()
{
    Log("End!");
    UCommunication::GetInstance().Stop();
    UNetwork::GetInstance().Stop();
}