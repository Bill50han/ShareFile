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
    UCommunication() = default; //这个类特殊在如果初始化失败，需要连带内核态程序一起重新加载。不能简单地像网络类那样崩溃然后重启了之。

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
