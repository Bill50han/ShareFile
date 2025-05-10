#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <iostream>
#include <Windows.h>
#include <cstdarg>

class Logger
{
private:
	FILE* logFile = NULL;
	bool ShowOnScreen = true;
	bool PrintToFile = false;

	bool ShowOnScreenIsChanged = false;

public:
	Logger(bool show = true, bool print = false, const char* path = NULL) : ShowOnScreen(show), PrintToFile(print)
	{
		if (show == false)
		{
			FreeConsole();
			ShowOnScreenIsChanged = true;
		}
		if (print == true && path != NULL)
		{
			logFile = fopen(path, "a+");
		}
	}
	~Logger()
	{
		if (logFile)
		{
			fclose(logFile);
		}
		if (ShowOnScreenIsChanged)
		{
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
		}
	}
	void operator()(const char* f, ...)
	{
		va_list arg;
		va_start(arg, f);
		if (ShowOnScreen)
		{
			vfprintf(stderr, f, arg);
		}
		if (PrintToFile)
		{
			vfprintf(logFile, f, arg);
			fflush(logFile);
		}
		va_end(arg);
	}
	void EnableShowOnScreen()
	{
		if (ShowOnScreen) return;

		ShowOnScreen = true;
		ShowOnScreenIsChanged = true;

		AllocConsole();
		freopen("CONIN$", "r+t", stdin);
		freopen("CONOUT$", "w+t", stdout);
		freopen("CONOUT$", "w+t", stderr);
	}
	bool EnablePrintToFile(const char* path)
	{
		if (PrintToFile)
		{
			fclose(logFile);
		}

		PrintToFile = true;

		logFile = fopen(path, "a+");
		if (logFile == NULL)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	void DisableShowOnScreen()
	{
		ShowOnScreen = false;
		ShowOnScreenIsChanged = true;

		FreeConsole();
		if (ShowOnScreenIsChanged)
		{
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
		}
	}
	void DisablePrintToFile()
	{
		PrintToFile = false;
		if (logFile != NULL)
		{
			fclose(logFile);
		}
	}
};

extern Logger logger;

#define Log(format, ...) \
    logger("%s:%d [%s] " format "\n", __FILE__, __LINE__, __FUNCSIG__ , ##__VA_ARGS__)
