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

			size_t WriteOffset;

			unsigned char PathAndWriteBuffer[1];	//ͬ��
		} Write;

		struct
		{
			size_t length;
			wchar_t path[1]; //���path��������wchar[]��
		} Delete;
	};
} Message, * PMessage;
