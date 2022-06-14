#include "pch.h"
#include "Runtime.h"

#include "imgui.h"
#include "Win32.h"
#include "VulkanGpu.h"
#include "VulkanUtils.h"

#include "Editor/suzanne.h"
#include "vectormath.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TimerCollection* TimerCollection::collection;

namespace Runtime
{

	bool Runtime::Running = true;

	struct UBO
	{
		Math::Mat4 Projection;
		Math::Mat4 View;
	};

	struct Model
	{
		Math::Mat4 Model;
	};

	void Runtime::Main()
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::StyleColorsDark();

		Win32MakeWindow();

		Graphics::Gpu::Create();
		
		std::vector<std::pair<int, void*>> vertexArray = {
			{(int)sizeof(suzannePositions), suzannePositions},
			{(int)sizeof(suzanneTextureCoords), suzanneTextureCoords},
			{(int)sizeof(suzanneNormals), suzanneNormals} 
		};
		auto suzanne = Graphics::Gpu::CreateVertexBuffer(vertexArray, suzanneIndices, (int)sizeof(suzanneIndices));

		Graphics::ShaderLayout layout;
		layout.SetUniform<UBO>(0);
		layout.SetUniform<Graphics::Texture>(1);
		layout.SetConstant<Model>();

		auto shader = Graphics::Gpu::CreateShaderFromFile(layout, "shader.vert.glsl", "shader.frag.glsl");

		int w, h, c;
		auto pixels = stbi_load("uv-test.png", &w, &h, &c, 4);
		auto texture = Graphics::Gpu::CreateTexture(pixels, w, h);
		
		auto ubo = Graphics::Gpu::CreateUniformSet(shader, layout);
		Graphics::Gpu::UpdateUniformSet(1, ubo, texture);

		int frameCount = 0;
		int fps = 0;
		auto LastTime = std::chrono::high_resolution_clock::now();
		
		auto rotation = 0.0f;

		while (Running)
		{
			ScopeTimer timer("Main Loop");
			
			Win32ProcessEvents();

			ImGui::NewFrame();
			ImGui::Begin("Debug");
			ImGui::Text("fps %d", fps);
			ImGui::Text("Scope Times");
			TimerCollection::get()->forEach([](std::pair<const char *, float> pair) {
				ImGui::Text("%-20s %.4f", pair.first, pair.second / 1000.0f);
			});
			ImGui::End();
			ImGui::Render();

			auto& buffer = Graphics::Gpu::NextCommandBuffer();

			buffer.StartRenderPass(Graphics::Gpu::GetSwapchainBuffer());

			buffer.BindShader(shader);

			UBO test{};
			test.Projection = Math::Mat4::Perspective(1600, 900, 60.0f, 0.1f, 100.0f);
			test.View = Math::Mat4::Translate(0, 0, -10.0f);
			Graphics::Gpu::UpdateUniformSet(0, ubo, test);
			buffer.BindUniformSet(0, ubo);

			Model model{};
			model.Model = Math::Mat4::Scale(1.0f + sin(rotation) / 2.0f);
			model.Model *= Math::Mat4::RotateY(3.1415f + rotation);
			rotation += 0.01f;
			buffer.BindConstant(model);

			buffer.Draw(suzanne);

			model.Model = Math::Mat4::Scale(1.0f + sin(rotation) / 2.0f);
			model.Model *= Math::Mat4::RotateY(3.1415f + rotation);
			model.Model *= Math::Mat4::Translate(5.0f, 0.0f, 0.0f);
			buffer.BindConstant(model);

			buffer.Draw(suzanne);

			buffer.RenderImGui();

			buffer.EndRenderPass(Graphics::Gpu::GetSwapchainBuffer());

			Graphics::Gpu::Submit();

			auto deltaTime = std::chrono::high_resolution_clock::now().time_since_epoch() - LastTime.time_since_epoch();
			if (deltaTime.count() >= 1000000000L) // 1 second
			{
				fps = frameCount;
				frameCount = 0;
				LastTime = std::chrono::high_resolution_clock::now();
			}
			else
			{
				frameCount++;
			}
		}

		Win32Exit();

	}

}