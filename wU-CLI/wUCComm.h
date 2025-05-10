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

#include "USFMessage.h"

class UCommunication
{
private:
	HANDLE hPort = NULL;
	std::thread messageThread;
	void MessageThreadProc();
    UCommunication() = default; //����������������ʼ��ʧ�ܣ���Ҫ�����ں�̬����һ�����¼��ء����ܼ򵥵�����������������Ȼ��������֮��

public:
	static UCommunication& GetInstance()
	{
		static UCommunication instance;
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
