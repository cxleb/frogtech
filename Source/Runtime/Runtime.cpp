#include "pch.h"
#include "Runtime.h"

#include "imgui.h"
#include "Window.h"
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
		ImGuiIO& io = ImGui::GetIO(); 
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();

		Window::Create();

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

		float ts = 0;
		auto LastFrameTime = std::chrono::high_resolution_clock::now();
		
		auto rotation = 0.0f;

		while (Running)
		{
			Window::ProcessEvents();

			// note(caleb): should replace this with some range based iterator
			for (WindowEvent event = Window::GetNextEvent(); event.Type != WindowEvent::NO_EVENT; event = Window::GetNextEvent())
			{
				switch (event.Type)
				{
				case WindowEvent::WINDOW_RESIZE:
				{
					//int width, height;
					//Window::GetWindowSize(&width, &height);
					Graphics::Gpu::ResizeSwapchain();
					break;
				}

				}
			}

			ScopeTimer ml("Main Loop");

			auto& buffer = Graphics::Gpu::NextCommandBuffer();

			ImGui::NewFrame();

			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			window_flags |= ImGuiWindowFlags_NoBackground;
			ImGui::Begin("Dock", 0, window_flags);
			ImGui::PopStyleVar(3);

			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0, 0), dockspace_flags);

			ImGui::End();

			ImGui::Begin("Debug");
			ImGui::Text("fps %d", fps);
			ImGui::Text("time step %.2f", ts);
			ImGui::End();
			ImGui::Begin("Scope Timers");
			TimerCollection::get()->forEach([](std::pair<const char*, float> pair) {
				ImGui::Text("%-20s %.4f", pair.first, pair.second / 1000.0f);
				});
			ImGui::End();

			ImGui::Render();

			{
				ScopeTimer rc("Record Commands");
				buffer.StartRenderPass(Graphics::Gpu::GetSwapchainBuffer());

				buffer.BindShader(shader);

				u32 width, height;
				Graphics::Gpu::GetSwapchainBuffer().GetSize(&width, &height);

				UBO test{};
				test.Projection = Math::Mat4::Perspective(width, height, 60.0f, 0.1f, 100.0f);
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
			}

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

			deltaTime = std::chrono::high_resolution_clock::now().time_since_epoch() - LastFrameTime.time_since_epoch();
			LastFrameTime = std::chrono::high_resolution_clock::now();
			const u64 target = 1000000000L / 60L; // how many nano seconds we expect to pass for every frame, at 60fps
			ts = (float)deltaTime.count() / target;
		}

		Window::Destroy();
	}

}