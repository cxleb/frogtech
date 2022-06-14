#pragma once
#include "pch.h"

namespace Runtime
{
	namespace Graphics
	{
		using Material = unsigned int;
		using Model  = unsigned int;
		using Mesh   = unsigned int;
		
		class Renderer
		{
		public:
			static void Draw();
			static void Submit();
		};
	}
}