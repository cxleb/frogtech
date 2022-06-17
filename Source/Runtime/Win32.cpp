#include "pch.h"
//#include "runtime.h"
#include <windows.h>
#include "win32.h"

#include "Window.h"

#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_win32.h"

HWND WindowHandle;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	//WindowEventCallback* callback = reinterpret_cast<WindowEventCallback*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	switch (uMsg)
	{
	case WM_CREATE:
		//SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);
		break;
	case WM_CLOSE:
		//callback->RegisterEvent<WindowEvent::Close>();
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		exit(0);
		break;
	case WM_SIZE:
	case WM_SIZING:
		//callback->RegisterEvent<WindowEvent::Resize>();
		Runtime::Window::PushEvent({ 
			.Type = Runtime::WindowEvent::WINDOW_RESIZE,
			.Window = {}
		});
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return true;
}

void Win32GetWindowSize(int* width, int* height)
{
	RECT rect;
	GetClientRect(WindowHandle, &rect);
	*width = (int)(rect.right - rect.left);
	*height = (int)(rect.bottom - rect.top);
}

void Win32MakeWindow()
{
	WNDCLASSEXW windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = GetModuleHandle(NULL);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"GameContainerClass";
	RegisterClassExW(&windowClass);


	RECT windowRect = { 0, 0, 1600, 900 };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	WindowHandle = CreateWindow(
		TEXT("GameContainerClass"),
		TEXT("game"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		GetModuleHandle(NULL),
		nullptr
	);

	ImGui_ImplWin32_Init(WindowHandle);
	
	ShowWindow(WindowHandle, 1);
}

void Win32ProcessEvents()
{
	MSG msg;
	PeekMessage(&msg, WindowHandle, 0, 0, PM_REMOVE);
	//GetMessage(&msg, WindowHandle, 0, 0);
	TranslateMessage(&msg);
	DispatchMessage(&msg);

	ImGui_ImplWin32_NewFrame();
}

void Win32Exit()
{
	exit(0);
}