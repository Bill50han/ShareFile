#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#include "USFMessage.h"

class UNetwork
{
private:
    SOCKET CreateSocket(bool enableBroadcast);
    void ReceiverProc();
    void ProcessMessage(PMessage msg, size_t length);
    sockaddr_in GetBroadcastAddress();
    bool IsLocalAddress(in_addr target);

    std::atomic_bool m_running;
    std::thread m_receiverThread;
    SOCKET m_sendSock = INVALID_SOCKET;
    SOCKET m_recvSock = INVALID_SOCKET;
    int m_port;
    char* AdapterName = NULL;

    //µ¥ÀýÄ£Ê½
    UNetwork(int port = 12345) : m_port(port), m_running(false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~UNetwork()
    {
        //Stop();
        WSACleanup();
    }

public:
    static UNetwork& GetInstance()
    {
        static UNetwork instance;
        return instance;
    }

    bool Start();
    void Stop();
    int Broadcast(const PMessage msg);
};