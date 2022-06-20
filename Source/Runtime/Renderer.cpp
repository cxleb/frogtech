#include "pch.h"
#include "Renderer.h"
#include "VulkanGpu.h"

/*

	how the renderer works.

	there is a concept of a "model" "mesh" "material" and "draw list"

	a mesh is a just data on the gpu that will be used as vertex data
	a material is a shader + camera transforms + some rendering data
	a model is a just a list of mesh material pairs
	a draw list exists for every material, and contains a list for meshes

	the interface exposes the mesh, model and material as integers, this 
	is because the renderer might realloc the mesh, model or material data
	and the pointers might change.

	Other parts of the code will submit meshes or models to the draw lists
	a material will have a draw list, this is because the code will upload 
	material data to the shader ubo and then for each draw call the 
	transform is submitted as a push constant, this reduces the need to 
	allocate alot of memory. Calling submit or submitmesh is extremely 
	fast as all it does it copy the mesh index and the transform to the 
	draw list.

	All the rendering logic is done in the Draw function

*/

namespace Runtime
{
	namespace Graphics
	{
		struct MeshData
		{
			VertexBuffer MeshBuffer;
		};
		
		struct MaterialData
		{
			Shader MaterialShader;
			UniformSet Uniforms;
			Texture albedo; // hard coding textures here, not sure what other textures we will need for the game
		};

		struct ModelData
		{
		};

		struct DrawList
		{

		};

		struct RendererData 
		{
			std::vector<DrawList>		DrawData;
			std::vector<ModelData>		Meshes;
			std::vector<MeshData>		Models;
			std::vector<MaterialData>	Materials;

		};
		static RendererData data;

		Material Renderer::CreateMaterial()
		{

		}

		Model Renderer::CreateModel()
		{
		}

		Mesh Renderer::CreateMesh()
		{
		}

		void Renderer::Draw()
		{
			auto commandBuffer = Gpu::NextCommandBuffer();

			commandBuffer.StartRenderPass(Gpu::GetSwapchainBuffer());

			//commandBuffer.

			commandBuffer.EndRenderPass(Gpu::GetSwapchainBuffer());

			Gpu::Submit();
		}

		// this submits a mesh to the draw list
		void Renderer::SubmitMesh(const Mesh mesh, const Material material, const Math::Transform& transform)
		{
		}

		// this submits a model to the draw list
		void Renderer::Submit(const Model mesh, const Math::Transform& transform)
		{
		}
	}
}