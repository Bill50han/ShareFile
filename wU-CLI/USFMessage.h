#pragma once

#include <Windows.h>

enum sfresult : ULONG
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
