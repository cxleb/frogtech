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
		layout.SetConstant<Math::Mat4>();

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

			Math::Transform t;
			t.Position = Math::Vector(2.0f, 0, 0);
			t.Scale = Math::Vector(1, 1, 1);
			t.Rotation = Math::Quaternion::FromEuler(0, rotation, 3.1415f);
			auto m = Math::Mat4::CreateTransform(t);
			buffer.BindConstant(m);

			buffer.Draw(suzanne);

			t.Position = Math::Vector(-2.0f, 0, 0);
			//t.Scale = Math::Vector(1, 1, 1);
			t.Rotation = Math::Quaternion::FromEuler(0, 3.1415f + rotation, 3.1415f);
			m = Math::Mat4::CreateTransform(t);
			buffer.BindConstant(m);

			buffer.Draw(suzanne);

			buffer.RenderImGui();

			buffer.EndRenderPass(Graphics::Gpu::GetSwapchainBuffer());

			Graphics::Gpu::Submit();

			rotation += 0.01f;

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