#pragma once
#include "pch.h"
#include "Math/Math.h"
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
			static Material CreateMaterial();
			static Model	CreateModel();
			static Mesh		CreateMesh();

			static void Draw();
			static void SubmitMesh(const Mesh mesh, const Material material, const Math::Transform& transform);
			static void Submit(const Model mesh, const Math::Transform& transform);
		};
	}
}