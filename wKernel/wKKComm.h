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
	KCommunication() = default;		//这个类的初始化依赖wKentry中的内容，但又是单例模式，且kernel环境不支持全局类的构造与析构函数，因此不使用自带的构造与析构函数
	static KCommunication instance;

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

				unsigned char WriteBuffer[1];	//同上
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

	//以下三个为注册到windows内核的回调函数，因此使用静态成员函数。
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
	// 禁止拷贝和赋值
	KCommunication(const KCommunication&) = delete;
	KCommunication& operator=(const KCommunication&) = delete;

	NTSTATUS Init();
	void Close();

};
