#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#define OVR_D3D_VERSION 11

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <stddef.h>

#include <chrono>
#include <cmath>
#include <deque>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <functional>
#include <condition_variable>

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <Shlwapi.h> // for PathRemoveFileSpec
#include <fileapi.h> // for GetFileTime function
#include <sys/stat.h>
#include <shellapi.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <dxgi1_3.h>

#include <OVR.h>
#include <OVR_CAPI_D3D.h>

#include <al.h>
#include <alc.h>

extern "C"
{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavresample/avresample.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
}


#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")
