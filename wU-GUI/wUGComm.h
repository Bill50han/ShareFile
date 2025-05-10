#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <cstring>
#include <clocale>
#include <cwchar>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fltUser.h>
#pragma comment(lib, "fltLib.lib")

#include "../wU-CLI/USFMessage.h"

class UCommunication
{
private:
	HANDLE hPort = NULL;
	UCommunication() = default; //����������������ʼ��ʧ�ܣ���Ҫ�����ں�̬����һ�����¼��ء����ܼ򵥵�����������������Ȼ��������֮��

	static UCommunication instance;
public:
	static UCommunication& GetInstance()
	{
		return instance;
	}
	int Start();
	void Stop()
	{
		if (hPort != NULL)
		{
			CloseHandle(hPort);
		}
	}
	int SendUCommMessage(PMessage msg);
};
