#include "pch.h"
#include "Renderer.h"

namespace Runtime
{
	namespace Graphics
	{
		struct RendererData 
		{
			std::unordered_map<Material, std::vector<Model>> DrawData;
		};
		static RendererData data;

		void Renderer::Draw()
		{

		}

		void Renderer::Submit()
		{

		}
	}
}