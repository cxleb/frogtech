#pragma once

namespace Runtime 
{
	struct WindowEvent
	{
		enum
		{
			NO_EVENT,
			KEY_DOWN,
			KEY_UP,
			KEY_TYPED,
			MOUSE_POS,
			MOUSE_BTNDOWN,
			MOUSE_BTNUP,
			WINDOW_RESIZE,
			WINDOW_MINIMISE,
			WINDOW_FOCUS,
			WINDOW_CLOSE,
		} Type;

		union
		{
			struct {

			} Key;
			struct {

			} Mouse;
			struct {

			} Window;
		};
	};

	class Window
	{
	public:
		static void Create();
		static void Destroy();

		// for use by the platform api
		static void PushEvent(WindowEvent event);
		
		// for use by the runtime
		static void ProcessEvents();
		static WindowEvent GetNextEvent();
		static void GetWindowSize(int* width, int* height);
	};

}
