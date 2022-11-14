﻿#pragma once

// Windows ヘッダー ファイル
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4995)
#define _WINSOCKAPI_
#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <assert.h>
#include <string>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iostream>
#include <WindowsX.h>	// for GET_X_LPARAM(), GET_Y_LPARAM()
#include <iomanip>		// for マニュピレーター
#include <crtdbg.h>		// for _CrtSetDbgFlag()等のデバッグ用関数
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include <memory>		// for std::unique_ptr on VC2015.
#pragma warning(pop)
#endif // _WIN32

#ifdef __linux
#include "forlinux.h"
#endif

#ifdef _UNICODE
typedef std::wstring tstring;
typedef std::wifstream tifstream;
typedef std::wostringstream tostringstream;
typedef std::wistringstream tistringstream;
#else
typedef std::string tstring;
typedef std::ifstream tifstream;
typedef std::ostringstream tostringstream;
typedef std::istringstream tistringstream;
#endif

#if defined(_WIN32) && defined(CHECK_LEAK_MEMORY)
	#define NNEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
	#define NNEW new
#endif

#define NDELETE(p) 		{if(p!=nullptr){delete (p);(p)=nullptr;}}
#define NDELETEARRAY(p) {if(p!=nullptr){delete[] (p);(p)=nullptr;}}

#define FOREGROUND_WHITE		(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE)

#ifdef NDEBUG
	#define DEBUG_BREAK		((void)0)
#else
	#ifdef _WIN32
		#define DEBUG_BREAK		DebugBreak()
	#elif __linux
		#define DEBUG_BREAK		assert(false)
	#else
		#define DEBUG_BREAK		((void)0)
	#endif
#endif


