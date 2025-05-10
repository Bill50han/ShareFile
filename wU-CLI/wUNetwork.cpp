#include "wUNetwork.h"
#include "wUCComm.h"
#include "USFMessage.h"
#include "USFLog.h"

bool UNetwork::Start()
{
    Log("���翪ʼ");
    m_running = true;
    m_receiverThread = std::thread(&UNetwork::ReceiverProc, this);
    m_sendSock = CreateSocket(true);

    return m_sendSock != INVALID_SOCKET;
}

void UNetwork::Stop()
{
    Log("����ֹͣ");
    m_running = false;
    if (m_receiverThread.joinable())
    {
        m_receiverThread.join();
    }
    closesocket(m_sendSock);
    closesocket(m_recvSock);
}

int UNetwork::Broadcast(const PMessage msg)
{
    sockaddr_in bcAddr = GetBroadcastAddress();
    bcAddr.sin_port = htons(m_port);

    return sendto(m_sendSock, (const char*)msg, static_cast<int>(msg->size), 0, (sockaddr*)&bcAddr, sizeof(bcAddr)) != SOCKET_ERROR;
}

SOCKET UNetwork::CreateSocket(bool enableBroadcast)
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    // �����ַ����
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    // ������ߵȼ�QoS
    int dscp = 46;  // EF��DSCPֵ 101110b
    int tos = (dscp << 2);  // ����2λ
    setsockopt(sock, IPPROTO_IP, IP_TOS, (char*)&tos, sizeof(tos));

    if (enableBroadcast)
    {
        int broadcastEnable = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));
    }

    // �󶨽����׽���
    if (!enableBroadcast)
    {
        sockaddr_in localAddr{};
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(m_port);
        localAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
        {
            closesocket(sock);
            return INVALID_SOCKET;
        }
    }

    return sock;
}

void UNetwork::ReceiverProc()
{
    m_recvSock = CreateSocket(false);
    if (m_recvSock == INVALID_SOCKET) return;

    char buffer[8192];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    while (m_running)
    {
        int received = recvfrom(m_recvSock, buffer, sizeof(buffer), 0, (sockaddr*)&fromAddr, &fromLen);
        if (received <= 0) continue;

        // �����Լ����͵���Ϣ
        if (IsLocalAddress(fromAddr.sin_addr)) continue;

        ProcessMessage(reinterpret_cast<Message*>(buffer), received);
    }
}

void UNetwork::ProcessMessage(PMessage msg, size_t length)
{
    if (length < sizeof(Message) || msg->size != length)
    {
        Log("Invalid message received");
        return;
    }

    switch (msg->type)
    {
    case M_ADD_PATH:
    case M_DEL_PATH:
    case M_QUERY_PATH:
    {
        UCommunication::GetInstance().SendUCommMessage(msg);
        break;
    }
    case M_HEART_BEAT:
    {
        break;
    }
    // ������Ϣ���ʹ���...
    default:
        std::cout << "�յ�δ֪������Ϣ: " << msg->type << "";
    }
}

sockaddr_in UNetwork::GetBroadcastAddress()
{
    sockaddr_in bcAddr{};
    bcAddr.sin_family = AF_INET;
    bcAddr.sin_addr.s_addr = inet_addr("192.168.1.255");    //��ֹ���������һ����ȷ���������ĳ�ʼֵ
    ULONG prefix = inet_addr("255.255.255.0");

    if (AdapterName != NULL)
    {
        PIP_ADAPTER_ADDRESSES adatpers = nullptr;
        ULONG size = 0;
        GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &size);
        adatpers = (PIP_ADAPTER_ADDRESSES)malloc(size);
        ULONG r = GetAdaptersAddresses(AF_INET, 0, NULL, adatpers, &size);
        if (r != NO_ERROR)
        {
            return bcAddr;
        }

        for (PIP_ADAPTER_ADDRESSES i = adatpers; i != NULL; i = i->Next)
        {
            if (i->OperStatus != IfOperStatusUp) continue;

            if (!strcmp(i->AdapterName, AdapterName))
            {
                if (i->FirstUnicastAddress != NULL)
                {
                    ULONG ip = ((PSOCKADDR_IN)(adatpers->FirstUnicastAddress->Address.lpSockaddr))->sin_addr.s_addr;
                    if (i->FirstPrefix != NULL)
                    {
                        prefix = ((PSOCKADDR_IN)(adatpers->FirstPrefix->Address.lpSockaddr))->sin_addr.s_addr;
                    }
                    bcAddr.sin_addr.s_addr = ip | (~prefix);
                }
            }
        }

        free(adatpers);
    }
    else
    {
        char localName[256];
        gethostname(localName, sizeof(localName));
        hostent* p = gethostbyname(localName);
        ULONG ip = ((in_addr*)(p->h_addr))->s_addr;
        //std::cout << inet_ntoa(*(in_addr*)(p->h_addr));
        bcAddr.sin_addr.s_addr = ip | (~prefix);
    }

    return bcAddr;
}

bool UNetwork::IsLocalAddress(in_addr target)
{
    //return false;   //just for test, need to be commented
    char localName[256];
    gethostname(localName, sizeof(localName));

    hostent* he = gethostbyname(localName);
    if (!he) return false;

    for (int i = 0; he->h_addr_list[i]; ++i)
    {
        in_addr* addr = reinterpret_cast<in_addr*>(he->h_addr_list[i]);
        if (addr->S_un.S_addr == target.S_un.S_addr)
        {
            return true;
        }
    }
    return false;
}
