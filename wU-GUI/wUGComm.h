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
	UCommunication() = default; //这个类特殊在如果初始化失败，需要连带内核态程序一起重新加载。不能简单地像网络类那样崩溃然后重启了之。

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
