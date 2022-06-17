#include "pch.h"
#include "Window.h"

#include "Win32.h"

namespace Runtime
{

	struct WindowData
	{
		// circular buffer
		WindowEvent* Events;
		int Write, Read, End;
	};
	WindowData Data;

	void Runtime::Window::Create()
	{
		Data.Events = new WindowEvent[200];
		Data.End = 200;
		
		Win32MakeWindow();
	}

	void Runtime::Window::Destroy()
	{
		delete[] Data.Events;

		Win32Exit();
	}

	void Window::PushEvent(WindowEvent event)
	{
		Data.Events[Data.Write++] = event;
		if (Data.Write >= Data.End)
			Data.Write = 0;
	}

	void Window::ProcessEvents()
	{
		Win32ProcessEvents();
	}

	WindowEvent Runtime::Window::GetNextEvent()
	{
		if (Data.Read == Data.Write)
		{
			WindowEvent event;
			event.Type = WindowEvent::NO_EVENT;
			return event;
		}
		int index = Data.Read++;

		if (Data.Read >= Data.End)
			Data.Read = 0;

		return Data.Events[index];
	}
	
	void Window::GetWindowSize(int* width, int* height)
	{
		Win32GetWindowSize(width, height);
	}
}