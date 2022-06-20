#include "pch.h"


#include <Windows.h>

#include "Runtime\Runtime.h"

//int WINAPI WinMain(
//	_In_ HINSTANCE hInstance,
//	_In_opt_ HINSTANCE hPrevInstance,
//	_In_ LPSTR lpCmdLine,
//	_In_ int nShowCmd)
//{
//
//	
//
//	MessageBoxA(NULL, "WinMain called me!", "WinMain", MB_OK);
//	
//	return EXIT_SUCCESS;
//}

#include "Runtime/Physics.h"

int main()
{
	Runtime::Runtime::Main();
	
	//int count = 1;
	//{
	//	ScopeTimer timer("Method One");
	//	for (int i = 0; i < count; i++)
	//	{
	//	}
	//}
	//{
	//	ScopeTimer timer("Method Two");
	//	for (int i = 0; i < count; i++)
	//	{
	//	}
	//}
	//TimerCollection::get()->forEach([](std::pair<const char*, float> pair) {
	//	printf("%-20s %.4f\n", pair.first, pair.second / 1000.0f);
	//});

	return EXIT_SUCCESS;
}