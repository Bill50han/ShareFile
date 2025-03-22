#pragma once
#include "wKutilities.h"
#include <fltKernel.h>
#include <ntifs.h>
#include <ntddk.h>
#include <dontuse.h>
#include "wKFilters.h"

#pragma warning( disable : 4100 4716 )

class KCommunication
{
private:
	KCommunication() = default;		//�����ĳ�ʼ������wKentry�е����ݣ������ǵ���ģʽ����kernel������֧��ȫ����Ĺ�����������������˲�ʹ���Դ��Ĺ�������������
	static KCommunication instance;

	enum MessageType : LONG64
	{
		M_HEART_BEAT,	//������
		M_RESULT,		//�����ķ���ֵ

		M_ADD_PATH,		//��ӦDatabase::add
		M_DEL_PATH,		//��ӦDatabase::del
		M_QUERY_PATH,	//��ӦDatabase::isIn

		M_FILE_CREATE,	//��Ӧfilter create�ص�
		M_FILE_WRITE,	//��Ӧfilter write�ص�
		M_FILE_DEL,		//��Ӧfilter setinfo�ص����del���

		M_KERNEL_ERROR,	//�ں˳���������������
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

			struct	//���������ṹ������ֱ�Ӷ�Ӧwindows api ZwCreateFile��ZwWriteFile�Ĳ�������˲���΢��ı�������ϰ��
			{
				ACCESS_MASK DesiredAccess;
				LARGE_INTEGER AllocationSize;
				USHORT FileAttributes;
				USHORT ShareAccess;
				ULONG CreateDisposition;
				ULONG CreateOptions;
				ULONG EaLength;

				size_t EaOffset;

				unsigned char PathAndEaBuffer[1];	//����ֻ�ǰ�unsigned char��byte�ã�path��wchar��ea��Ҳ������0��
				//����������С��������size�򱾲��EaOffset��EaLength˫���չ�ͬȷ����
			} Create;

			struct
			{
				ULONG Length;
				ULONG Key;
				LARGE_INTEGER ByteOffset;

				unsigned char WriteBuffer[1];	//ͬ��
			} Write;

			struct
			{
				size_t length;
				wchar_t path[1];
			} Delete;
		};
	} Message, * PMessage;

	typedef struct _CLIENT_PORT_CONTEXT {
		PFLT_PORT ClientPort;
	} CLIENT_PORT_CONTEXT, * PCLIENT_PORT_CONTEXT;

	PFLT_PORT MFCommServerPort = NULL;

	sfresult SendKCommMessage(PMessage message);

	//��������Ϊע�ᵽwindows�ں˵Ļص����������ʹ�þ�̬��Ա������
	static NTSTATUS MessageNotifyCallback(
			__in_opt PVOID PortCookie,
			__in_opt PVOID InputBuffer,
			__in ULONG InputBufferLength,
			__out_opt PVOID OutputBuffer,
			__in ULONG OutputBufferLength,
			__out PULONG ReturnOutputBufferLength
		);

	static VOID DisconnectNotifyCallback(
			__in_opt PVOID ConnectionCookie
		);

	static NTSTATUS ConnectNotifyCallback(
			__in PFLT_PORT ClientPort,
			__in PVOID ServerPortCookie,
			__in_bcount(SizeOfContext) PVOID ConnectionContext,
			__in ULONG SizeOfContext,
			__out PVOID* ConnectionCookie
		);

public:
	static KCommunication& GetInstance()
	{
		return instance;
	}
	// ��ֹ�����͸�ֵ
	KCommunication(const KCommunication&) = delete;
	KCommunication& operator=(const KCommunication&) = delete;

	NTSTATUS Init();
	void Close();

};
