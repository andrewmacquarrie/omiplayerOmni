#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

using namespace std;

// maximum mumber of lines the output console should have
static const WORD MAX_CONSOLE_LINES = 500;

class Console
{
public:
	static void RedirectIOToConsole();

private:
	Console();
	~Console();
};
