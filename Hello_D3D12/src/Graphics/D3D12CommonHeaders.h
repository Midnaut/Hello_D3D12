#pragma once

#include <dxgi1_6.h>
#include <D3DCompiler.h>
#include <d3d12.h>

#ifdef _DEBUG
#include <d3d12sdklayers.h>
#endif

#include <wrl.h>
#include <spdlog/spdlog.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

// Assert that COM call to D3D API succeeded
#ifdef _DEBUG
#ifndef DXCall
// converts line number to string then outpts a debug string into VS Output Panel.
#define DXCall(x)									\
if(FAILED(x)) {										\
	char line_number[32];							\
	sprintf_s(line_number, "%u", __LINE__);			\
	OutputDebugStringA("ERROR in: ");				\
	OutputDebugStringA(__FILE__);					\
	OutputDebugStringA("\nLine: ");					\
	OutputDebugStringA(line_number);				\
	OutputDebugStringA("\n");						\
	OutputDebugStringA(#x);							\
	OutputDebugStringA("\n");						\
	__debugbreak();									\
}									
#endif // !DXCall
#else
#ifndef DXCall
#deinfe DXCall(x) x
#endif // !DXCall
#endif // _DEBUG


#ifdef _DEBUG
// sets the name of the COM object and outpts a debug string into VS Output Panel.
#define NAME_D3D12_OBJECT(obj, name)				\
obj->SetName(name);									\
OutputDebugString(name);							\
OutputDebugString(L"\n");
#else
#define NAME_D3D12_OBJECT(x, name)
#endif // _DEBUG

