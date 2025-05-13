#include "wUNetwork.h"
#include "wUCComm.h"
#include "USFMessage.h"
#include "USFLog.h"


std::wstring DosPathToNtPath(const std::wstring& strPath)
{
    std::wstring strResultPath;
    TCHAR szDriveStrings[MAX_PATH] = { 0 };
    TCHAR szDosBuf[MAX_PATH] = { 0 };
    LPTSTR pDriveStr = NULL;

    do
    {
        // ��ȡ�̷���������
        if (!::GetLogicalDriveStringsW(_countof(szDriveStrings), szDriveStrings))
        {
            break;
        }

        // �����̷���
        for (int i = 0; i < _countof(szDriveStrings); i += 4)
        {
            pDriveStr = &szDriveStrings[i];
            pDriveStr[2] = L'\0';

            // ��ѯ�̷���Ӧ��DOS�豸����
            DWORD dwCch = ::QueryDosDeviceW(pDriveStr, szDosBuf, _countof(szDosBuf));
            if (!dwCch)
            {
                break;
            }

            // ��β�� 2 �� NULL, ��ȥ 2 ����ַ�����
            if (dwCch >= 2)
            {
                dwCch -= 2;
            }

            if (strPath.size() < dwCch)
            {
                break;
            }

            // ·��ƴ��
            if (L'\\' == strPath[dwCch] && 0 == wcsncmp(strPath.c_str(), szDosBuf, dwCch))
            {
                strResultPath = pDriveStr;
                strResultPath += &strPath[dwCch];
                break;
            }
        }

    } while (false);

    return strResultPath;
}

bool UNetwork::Start()
{
    Log("���翪ʼ");

    std::wstring buffer;
    std::wfstream o("adapter.conf", std::ios::in);
    if (o.is_open())
    {
        o.imbue(std::locale(std::locale(), "", LC_CTYPE));
        std::getline(o, buffer);
        o.close();

        Log("ʹ���豸��%ws", buffer.c_str());
    }
    else
    {
        Log("δ�����豸");
    }

    AdapterFriendlyName = new wchar_t[buffer.length() + 1];
    wcscpy(AdapterFriendlyName, buffer.c_str());

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
    if (AdapterFriendlyName) delete[] AdapterFriendlyName;
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

        Log("�յ�ȫ�������");

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
        Log("�յ����������k����");
        UCommunication::GetInstance().SendUCommMessage(msg);
        break;
    }
    case M_FILE_WRITE:
    {
        std::wstring p = std::wstring((WCHAR*)msg->Write.PathAndWriteBuffer) + L"." + std::to_wstring(std::time(0));
        std::wstring dp = DosPathToNtPath(p);
        Log("�� \"%ws\" д�����ݣ�(aka. %ws)", p.c_str(), dp.c_str());
        std::wfstream w(dp.c_str(), std::ios::out | std::ios::binary);
        //w.seekp(msg->Write.ByteOffset.QuadPart, std::ios::beg);
        w.imbue(std::locale(std::locale(), "", LC_CTYPE));
        w << msg->Write.ByteOffset.QuadPart << L"\r\n";
        w.write((WCHAR*)(msg->Write.PathAndWriteBuffer + msg->Write.WriteOffset), msg->Write.Length);
        w.flush();
        w.close();
        break;
    }
    case M_FILE_CREATE:
    case M_FILE_DEL:
    case M_HEART_BEAT:
    {
        Log("�յ����������f����");
        break;
    }
    // ������Ϣ���ʹ���...
    default:
        Log("��������Ĳ�������δ֪");
    }
}

sockaddr_in UNetwork::GetBroadcastAddress()
{
    sockaddr_in bcAddr{};
    bcAddr.sin_family = AF_INET;
    bcAddr.sin_addr.s_addr = inet_addr("192.168.1.255");    //��ֹ���������һ����ȷ���������ĳ�ʼֵ
    ULONG prefix = inet_addr("255.255.255.0");

    if (AdapterFriendlyName != NULL)
    {
        Log("���÷���1");
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

            if (!wcscmp(i->FriendlyName, AdapterFriendlyName))
            {
                Log("�ҵ�ѡ�е�������");
                if (i->FirstUnicastAddress != NULL)
                {
                    ULONG ip = ((PSOCKADDR_IN)(adatpers->FirstUnicastAddress->Address.lpSockaddr))->sin_addr.s_addr;
                    Log("���õĵ�����ַ��%s", inet_ntoa(((PSOCKADDR_IN)(adatpers->FirstUnicastAddress->Address.lpSockaddr))->sin_addr));
                    if (i->FirstPrefix != NULL)
                    {
                        prefix = ((PSOCKADDR_IN)(adatpers->FirstPrefix->Address.lpSockaddr))->sin_addr.s_addr;
                        Log("���ڵ�IP prefix��%s", inet_ntoa(((PSOCKADDR_IN)(adatpers->FirstUnicastAddress->Address.lpSockaddr))->sin_addr));
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
        Log("���÷���2��%s : %s", localName, inet_ntoa(*((in_addr*)(p->h_addr))));
        //std::cout << inet_ntoa(*(in_addr*)(p->h_addr));
        bcAddr.sin_addr.s_addr = ip | (~prefix);
    }

    Log("��ǰ�Ĺ㲥��ַ��%s", inet_ntoa(bcAddr.sin_addr));

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
