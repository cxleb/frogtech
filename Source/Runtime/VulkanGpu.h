#pragma once
#include "common.h"

#include <vector>
#include <map>

#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

namespace Runtime
{
	namespace Graphics
	{
		const u64 FramesInFlight =  3;

		class Texture
		{
			friend class Gpu;
			friend class CommandBuffer;

			VkImage Image;
			VkDeviceMemory Memory;
			VkImageView ImageView;
			VkSampler Sampler;
		};

		class ShaderLayout
		{
			friend class Gpu;
		
		private:
			void assure(u32 index);

		public:
			ShaderLayout()
				: ConstantSize(0)
			{
			}

			template<typename T>
			void SetUniform(u32 index)
			{
				assure(index);

				Descriptor descriptor = {
					.Set = true,
					.Type = DescriptorType::Buffer,
					.Size = sizeof(T)
				};
				DescriptorSet[index] = descriptor;

			}

			template<>
			void SetUniform<Texture>(u32 index)
			{
				assure(index);

				Descriptor descriptor = {
					.Set = true,
					.Type = DescriptorType::Texture,
					.Size = 0
				};
				DescriptorSet[index] = descriptor;

			}

			template <typename T>
			void SetConstant()
			{
				ConstantSize = sizeof(T);
			}

		private:
			enum class DescriptorType
			{
				Texture,
				Buffer
			};
			struct Descriptor
			{

				b8 Set;
				DescriptorType Type;
				u64 Size;
			};
			std::vector<Descriptor> DescriptorSet;
			u32 ConstantSize;

		};

		class Shader
		{
			friend class Gpu;
			friend class CommandBuffer;

			u32 Index;
		};

		struct DescriptorPool
		{
			VkDescriptorPool Pool;
			int Count;
		};

		class ShaderProgram
		{
			friend class Gpu;
			friend class CommandBuffer;
		
			VkShaderModule VertexShader = nullptr;
			VkShaderModule FragmentShader = nullptr;
			VkDescriptorSetLayout Layout = nullptr;
			VkPipelineLayout PipelineLayout = nullptr;
			std::vector<DescriptorPool> DescriptorPools[FramesInFlight];
			std::vector<VkDescriptorPoolSize> PoolAllocateInfo;
			std::unordered_map<u64, VkDescriptorSet> DescriptorSetCache;
		};

		class UniformSet
		{
			friend class Gpu;
			friend class CommandBuffer;

			enum class UniformType
			{
				Texture,
				Buffer
			};

			struct Uniform
			{
				UniformType Type;
				std::vector<VkBuffer> Buffers;
				std::vector<u64> BufferOffsets;
				// Texture would go here but the create uniform set function doesnt allocate that
			};

			VkDeviceMemory Memory;
			std::vector<Uniform> Uniforms;
			std::vector<VkDescriptorSet> DescriptorSets;

			size_t Size;
		};

		class RenderBuffer
		{
			friend class Gpu;
			friend class CommandBuffer;
			
			VkExtent2D RenderExtent;
			std::vector<VkImage> Images;
			std::vector<VkImageView> ImageViews;
			VkDeviceMemory DepthImageMemory;
			VkImage DepthImage;
			VkImageView DepthImageView;
			std::vector<VkFramebuffer> Framebuffers;
			VkRenderPass RenderPass;
		};

		class VertexBuffer
		{
			friend class Gpu;
			friend class CommandBuffer;

			VkDeviceMemory Memory;
			std::vector<VkBuffer> VertexBuffers;
			VkBuffer IndexBuffer;
			u32 indexCount;
		};

		class CommandBuffer
		{
			friend class Gpu;
			
			struct Binding
			{
				u32 index;
				UniformSet* buffer;
				Texture* texture;
			};

			struct State
			{
				VkDescriptorSet DescriptorSet;
				Shader Shader;
				std::vector<Binding> Bindings;
			};

		public:
			void StartRenderPass(RenderBuffer& buffer);
			void EndRenderPass(RenderBuffer& buffer);

			void RenderImGui();
			void BindUniformSet(u32 index, UniformSet& buffer);

			template<typename T>
			void BindConstant(T& constant)
			{
				auto layout = Gpu::GetPipelineLayout(Current.Shader);
				vkCmdPushConstants(Buffer, layout, VK_SHADER_STAGE_ALL, 0, sizeof(T), &constant);
			}

			void BindShader(Shader& shader);
			void Draw(VertexBuffer& buffer);

		private:
			u32 Index;
			u32 CurrentImage;
			VkCommandBuffer Buffer;
			VkFence Fence;
			State Current;
		};

		struct PipelineOptions
		{
			Shader Shader;
			size_t GetHash();
		};

		class Gpu
		{
		public:
			static CommandBuffer& NextCommandBuffer(b8 acquireImage = true);
			static void Submit(b8 present = true);
			static void Create();
			static void SyncGpu();
			static void ResizeSwapchain();

			static VertexBuffer CreateVertexBuffer(std::vector<std::pair<int, void*>>& vertexArrays, u32* indexArray, u32 indexSize);
			static Shader CreateShaderFromFile(ShaderLayout& layout, const std::string& vert, const std::string& frag);
			static UniformSet CreateUniformSet(Shader& shader, ShaderLayout& layout);
			static Texture CreateTexture(void* data, u32 width, u32 height);

			template<typename BufferType>
			static void UpdateUniformSet(u32 index, UniformSet& set, BufferType& contents)
			{
				void* mmap;
				uint64_t offset = set.Uniforms[index].BufferOffsets[FrameIterator];
				vkMapMemory(Device, set.Memory, offset, sizeof(BufferType), 0, &mmap);
				memcpy(mmap, &contents, sizeof(contents));
				vkUnmapMemory(Device, set.Memory);
			}

			template<>
			static void UpdateUniformSet(u32 index, UniformSet& set, Texture& texture)
			{
				for (int i = 0; i < FramesInFlight; i++)
				{
					VkDescriptorImageInfo imageInfo = {
						.sampler = texture.Sampler,
						.imageView = texture.ImageView,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					};
					
					VkWriteDescriptorSet descriptorWrite = {
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.pNext = nullptr,
						.dstSet = set.DescriptorSets[i],
						.dstBinding = index,
						.dstArrayElement = 0,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.pImageInfo = &imageInfo,
						.pBufferInfo = nullptr,
						.pTexelBufferView = nullptr,
					};

					vkUpdateDescriptorSets(Device, 1, &descriptorWrite, 0, nullptr);
				}
			}

			static RenderBuffer& GetSwapchainBuffer();

		// vulkan specific things
			static void ResizeTransferBuffer(u64 size);
			static void ResetPools();
			static VkPipeline GetPipeline(PipelineOptions& options);
			static VkPipelineLayout GetPipelineLayout(Shader& shader);
			static VkDescriptorSet AllocateDescriptor(Shader& s);
			
		private:
			static VkInstance Instance;
			static VkPhysicalDevice PhysicalDevice;
			static VkDevice Device;

			static VkQueue Queue;
			static u32 QueueFamily;
			static VkCommandPool CommandPool;

			static u32 FrameIterator;
			static CommandBuffer CommandBuffers[FramesInFlight];

			static VkSurfaceKHR Surface;
			
			// swapchain stuff
			static u32 ImageIndex;
			static VkSwapchainKHR Swapchain;
			static RenderBuffer SwapchainBuffer;
			static VkExtent2D SwapchainExtent;
			static std::vector<VkSemaphore> ImageAvailableSemaphores;
			static std::vector<VkSemaphore> RenderFinishedSemaphores;

			// pipeline stuff
			static std::vector<ShaderProgram> Shaders;
			static std::map<u64, VkPipeline> PipelineCache;

			// transfer buffer
			static VkBuffer TransferBuffer;
			static VkDeviceMemory TransferMemory;
			static u64 TransferSize;

			static VkDescriptorPool ImGuiDescriptorPool;
		};
	}

}
