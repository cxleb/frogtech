#include "pch.h"
// disable the "Prefer Scoped enum class", vulkan is written in C and there is no such thing.
#pragma warning( disable : 26812 )

// include before our own header to include windows.h
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

#include "VulkanGpu.h"

#include "backends/imgui_impl_vulkan.h"
#include "VulkanUtils.h"

#include <map>

// extern window handle from Win32.cpp
extern HWND WindowHandle;

// for now this is a function, otherwise this should be a macro that is removes this function
inline void VulkanCheck(VkResult result)
{
	if (result != VK_SUCCESS)
	{
		abort();
	}
}

// helper functions
VkFormat findImageFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
	
	return VK_FORMAT_UNDEFINED;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	return 0;

	//throw std::runtime_error("failed to find suitable memory type!");
}

namespace Runtime
{
	namespace Graphics
	{
		VkInstance Gpu::Instance;
		VkPhysicalDevice Gpu::PhysicalDevice;
		VkDevice Gpu::Device;

		u32 Gpu::QueueFamily;
		VkQueue Gpu::Queue;
		VkCommandPool Gpu::CommandPool;


		u32 Gpu::FrameIterator;
		CommandBuffer Gpu::CommandBuffers[FramesInFlight];

		VkSurfaceKHR Gpu::Surface;

		// Swapchain stuff
		u32 Gpu::ImageIndex;
		VkSwapchainKHR Gpu::Swapchain;
		RenderBuffer Gpu::SwapchainBuffer;
		VkExtent2D Gpu::SwapchainExtent;
		std::vector<VkSemaphore> Gpu::ImageAvailableSemaphores;
		std::vector<VkSemaphore> Gpu::RenderFinishedSemaphores;

		// pipeline stuff
		std::vector<ShaderProgram> Gpu::Shaders;
		std::map<u64, VkPipeline> Gpu::PipelineCache;

		// transfer buffer
		VkBuffer Gpu::TransferBuffer;
		VkDeviceMemory Gpu::TransferMemory;
		u64 Gpu::TransferSize;

		VkDescriptorPool Gpu::ImGuiDescriptorPool;

		// temp variables

		void Gpu::Create()
		{
			// Instance Creation
			{
				std::vector<const char*> enabledInstanceLayers;
				std::vector<const char*> enabledInstanceExtensions;

				enabledInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");

				enabledInstanceExtensions.push_back("VK_KHR_surface");
				enabledInstanceExtensions.push_back("VK_KHR_win32_surface");
				//enabledInstanceExtensions.push_back("VK_EXT_debug_utils");

				VkApplicationInfo applicationInfo{};
				applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
				applicationInfo.pNext = NULL;
				applicationInfo.pApplicationName = "";
				applicationInfo.applicationVersion = 1;
				applicationInfo.pEngineName = "";
				applicationInfo.engineVersion = 1;
				applicationInfo.apiVersion = VK_API_VERSION_1_2;

				VkInstanceCreateInfo instanceInfo;
				instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
				instanceInfo.pNext = NULL;
				instanceInfo.flags = 0;
				instanceInfo.pApplicationInfo = &applicationInfo;
				instanceInfo.enabledLayerCount = (u32)enabledInstanceLayers.size();
				instanceInfo.ppEnabledLayerNames = enabledInstanceLayers.data();
				instanceInfo.enabledExtensionCount = (u32)enabledInstanceExtensions.size();
				instanceInfo.ppEnabledExtensionNames = enabledInstanceExtensions.data();

				VulkanCheck(vkCreateInstance(&instanceInfo, NULL, &Instance));
			}

			// Physical Device Finding
			{
				u32 deviceCount = 0;
				std::vector<VkPhysicalDevice> physicalDevices;
				VulkanCheck(vkEnumeratePhysicalDevices(Instance, &deviceCount, nullptr));
				// weird sanity check
				if (deviceCount == 0) { exit(1); }
				physicalDevices.resize(deviceCount);
				VulkanCheck(vkEnumeratePhysicalDevices(Instance, &deviceCount, physicalDevices.data()));

				u32 physicalDeviceIndex = 0;
				VkPhysicalDeviceProperties deviceProperties;
				for (u32 i = 0; i < deviceCount; i++)
				{
					memset(&deviceProperties, 0, sizeof(deviceProperties));
					vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);

					if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					{
						physicalDeviceIndex = i;
						break;
					}
				}

				PhysicalDevice = physicalDevices[physicalDeviceIndex];
			}

			// Surface Creation
			{
				VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
				surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
				surfaceCreateInfo.pNext = nullptr;
				surfaceCreateInfo.flags = 0; // unsued
				surfaceCreateInfo.hinstance = NULL;
				surfaceCreateInfo.hwnd = WindowHandle;

				vkCreateWin32SurfaceKHR(Instance, &surfaceCreateInfo, nullptr, &Surface);
			}

			// Logical Device Creation
			{
				std::vector<const char*>  enabledDeviceExtensions;
				enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

				/////////////////////////////////////////////////////////////////////////
				// THIS IS A LOAD OF TRASH!!!!!!!!!!!!!!!!!!!!!!!!!!!
				// eventually id like dynamically dispatch queues to certains tasks
				// we would have to describe every task we have, however thats not possible now or ever atleast for this code
				// for now one queue that does everything and well try have one queue that does presentation, fuck you intel and amd
				// 
				// intel always has a single uber queue, going to be a fucking problem for, shitty laptops gonna have trash performance, we can try just spam the fuck out the gpu just back to back command buffers, become a little toaster...
				// nvidia seems to always have 16 uber queues and other families that do things...
				// amd is fucking shit

				u32 queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyCount, NULL);
				std::vector<VkQueueFamilyProperties> familyProperties(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyCount, familyProperties.data());

				u32 uberQueueFamily = 0;
				u32 uberQueue = 0; // always 0 lol
				
				for (u32 j = 0; j < queueFamilyCount; j++)
				{
					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, j, Surface, &presentSupport);
					if (familyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
						familyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT &&
						familyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT &&
						presentSupport)
					{
						// the uber queue family
						uberQueueFamily = j;
						break;
					}
				}

				std::vector<VkDeviceQueueCreateInfo> deviceQueueInfos;

				// kinda useless thing
				// this useless thing stops vulkan from having a spaz attack
				// why does it look like this? ask vulkan mate 
				f32 queuePriorities[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

				VkDeviceQueueCreateInfo graphicsDeviceQueueInfo;
				graphicsDeviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				graphicsDeviceQueueInfo.pNext = NULL;
				graphicsDeviceQueueInfo.flags = 0;
				graphicsDeviceQueueInfo.queueFamilyIndex = uberQueueFamily;
				graphicsDeviceQueueInfo.queueCount = 1;
				graphicsDeviceQueueInfo.pQueuePriorities = queuePriorities;
				
				deviceQueueInfos.push_back(graphicsDeviceQueueInfo);

				// no idea what this does but you need it
				VkPhysicalDeviceFeatures deviceFeatures;
				vkGetPhysicalDeviceFeatures(PhysicalDevice, &deviceFeatures);

				VkDeviceCreateInfo deviceInfo{};
				deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				deviceInfo.pNext = NULL;
				deviceInfo.flags = 0;
				// device layers are deprecated in later versions of vulkan
				// deviceInfo.enabledLayerCount = 0;
				// deviceInfo.ppEnabledLayerNames = nullptr;
				deviceInfo.enabledExtensionCount = (u32)enabledDeviceExtensions.size();
				deviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
				deviceInfo.pEnabledFeatures = &deviceFeatures;
				deviceInfo.queueCreateInfoCount = (u32)deviceQueueInfos.size();
				deviceInfo.pQueueCreateInfos = deviceQueueInfos.data();

				VulkanCheck(vkCreateDevice(PhysicalDevice, &deviceInfo, NULL, &Device));

				vkGetDeviceQueue(Device, uberQueueFamily, 0, &Queue);

				VkCommandPoolCreateInfo poolInfo{};
				poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				poolInfo.queueFamilyIndex = uberQueueFamily;
				poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

				VulkanCheck(vkCreateCommandPool(Device, &poolInfo, nullptr, &CommandPool));
			}

			// Command Buffer Creation
			{
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = CommandPool;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = (uint32_t)FramesInFlight;

				VkCommandBuffer commandBuffers[FramesInFlight];

				vkAllocateCommandBuffers(Device, &allocInfo, commandBuffers);


				VkFenceCreateInfo fenceInfo{};
				fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceInfo.pNext = nullptr;
				fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

				for (int i = 0; i < FramesInFlight; i++)
				{
					CommandBuffers[i].Buffer = commandBuffers[i];
					CommandBuffers[i].Index = i;

					vkCreateFence(Device, &fenceInfo, nullptr, &CommandBuffers[i].Fence);
				}
			}

			// Swapchain Creation
			{
				ResizeSwapchain();

				// the fences and the semaphores


				// create "how many frames in flight" semaphores 
				// we do this because we *cant* guarentee that the image aquisition is just (imageIndex + 1 % swapChainImages)
				// therefore we have to use the frames in flight to time the gpu
				// they use the *same* count as the command buffer fences but they should not be thought of as being together
				VkSemaphoreCreateInfo semaphoreInfo{};
				semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
				semaphoreInfo.flags = 0;
				semaphoreInfo.pNext = nullptr;
				RenderFinishedSemaphores.resize(FramesInFlight);
				ImageAvailableSemaphores.resize(FramesInFlight);
				for (uint32_t i = 0; i < FramesInFlight; i++)
				{
					vkCreateSemaphore(Device, &semaphoreInfo, nullptr, &RenderFinishedSemaphores[i]);
					vkCreateSemaphore(Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphores[i]);
				}
			}
		}

		void Gpu::SyncGpu()
		{
			// this function syncs the gpu to the cpu by waiting for it to stop doing any work
			vkDeviceWaitIdle(Device);
		}

		void Gpu::ResizeSwapchain()
		{
			if (Device == nullptr)
				return;

			SyncGpu();

			// firstly clear all the resources assoicated with the swapchain
			{
				//vkDestroyRenderPass(Device, SwapchainBuffer.RenderPass, nullptr);

				for (VkImageView view : SwapchainBuffer.ImageViews)
					vkDestroyImageView(Device, view, nullptr);
				
				vkFreeMemory(Device, SwapchainBuffer.DepthImageMemory, nullptr);
				vkDestroyImage(Device, SwapchainBuffer.DepthImage, nullptr);
				vkDestroyImageView(Device, SwapchainBuffer.DepthImageView, nullptr);

				for (VkFramebuffer framebuffer : SwapchainBuffer.Framebuffers)
					vkDestroyFramebuffer(Device, framebuffer, nullptr);
				
				vkDestroySwapchainKHR(Device, Swapchain, nullptr);

				SwapchainBuffer.Images.clear();
				SwapchainBuffer.ImageViews.clear();
				SwapchainBuffer.Framebuffers.clear();
			}

			// if your computer doesnt support this what the fuck
			VkSurfaceFormatKHR selectedSurfaceFormat = {
				// mmm some fancy c++20 features
				.format = VK_FORMAT_B8G8R8A8_UNORM,
				.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			};
			VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;// VK_PRESENT_MODE_IMMEDIATE_KHR; // NO V SYNC //  VK_PRESENT_MODE_FIFO_KHR
			VkFormat depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

			// Swapchain Creation
			{
				VkSurfaceCapabilitiesKHR surfaceCapabilities;
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &surfaceCapabilities);

				/*// get formats
				std::vector<VkSurfaceFormatKHR> surfaceFormats;
				u32 formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, nullptr);
				surfaceFormats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, surfaceFormats.data());

				// get present modes
				std::vector<VkPresentModeKHR> presentModes;
				u32 presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentModeCount, nullptr);
				presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentModeCount, presentModes.data());*/

				// well if we are not on windows this sorta doesnt work so i guess you die
				SwapchainExtent = surfaceCapabilities.currentExtent;
				SwapchainBuffer.RenderExtent = SwapchainExtent;

				u32 imageCount = 0;
				imageCount = surfaceCapabilities.minImageCount + 1;
				if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
					imageCount = surfaceCapabilities.maxImageCount;
				}

				// time to create the swapchain images
				VkSwapchainCreateInfoKHR swapChainCreateInfo = {
				   .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				   .pNext = nullptr,
				   .flags = 0, // no flags
				   .surface = Surface,
				   .minImageCount = imageCount,
				   .imageFormat = selectedSurfaceFormat.format,
				   .imageColorSpace = selectedSurfaceFormat.colorSpace,
				   .imageExtent = SwapchainExtent,
				   .imageArrayLayers = 1, // vulkan spec says this needs to be 1
				   .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				   .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
				   .queueFamilyIndexCount = 1, // BAD CODE BAD CODE
				   .pQueueFamilyIndices = &QueueFamily,
				   .preTransform = surfaceCapabilities.currentTransform,
				   .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				   .presentMode = presentMode,
				   .clipped = VK_TRUE,
				   .oldSwapchain = nullptr, // putting the old swapchain in break it??
				};

				VulkanCheck(vkCreateSwapchainKHR(Device, &swapChainCreateInfo, nullptr, &Swapchain));
			}

			// Get Images and Views
			{
				// get the images from the swapchain
				u32 imageCount;
				vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount, nullptr);
				SwapchainBuffer.Images.resize(imageCount);
				vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount, SwapchainBuffer.Images.data());

				// create image views out of the swapchain images
				SwapchainBuffer.ImageViews.reserve(imageCount);
				for (VkImage image : SwapchainBuffer.Images)
				{
					VkImageViewCreateInfo createInfo{};
					createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					createInfo.image = image;
					createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					createInfo.format = selectedSurfaceFormat.format;
					createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
					createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					createInfo.subresourceRange.baseMipLevel = 0;
					createInfo.subresourceRange.levelCount = 1;
					createInfo.subresourceRange.baseArrayLayer = 0;
					createInfo.subresourceRange.layerCount = 1;

					VkImageView imageView;
					vkCreateImageView(Device, &createInfo, nullptr, &imageView);
					SwapchainBuffer.ImageViews.push_back(imageView);
				}
			}

			// Create Depth Image, manually of course -.-
			{
				depthFormat = findImageFormat(PhysicalDevice, { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
				
				VkExtent3D extent
				{
					.width = SwapchainExtent.width,
					.height = SwapchainExtent.height,
					.depth = 1,
				};

				VkImageCreateInfo createInfo{
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = depthFormat,
					.extent = extent,
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.tiling = VK_IMAGE_TILING_OPTIMAL,
					.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
					.queueFamilyIndexCount = 0,
					.pQueueFamilyIndices = nullptr,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				};

				VulkanCheck(vkCreateImage(Device, &createInfo, nullptr, &SwapchainBuffer.DepthImage));
			
				VkMemoryRequirements memRequirements;
				vkGetImageMemoryRequirements(Device, SwapchainBuffer.DepthImage, &memRequirements);

				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = findMemoryType(PhysicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				VulkanCheck(vkAllocateMemory(Device, &allocInfo, nullptr, &SwapchainBuffer.DepthImageMemory));

				VulkanCheck(vkBindImageMemory(Device, SwapchainBuffer.DepthImage, SwapchainBuffer.DepthImageMemory, 0));
			}

			// Create Depth Image View
			{
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = SwapchainBuffer.DepthImage;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = depthFormat;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				VulkanCheck(vkCreateImageView(Device, &createInfo, nullptr, &SwapchainBuffer.DepthImageView));
			}

			if(SwapchainBuffer.RenderPass == nullptr)
			{

				VkAttachmentDescription attachments[2] =
				{
					{ // the colour one
						.format = selectedSurfaceFormat.format,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					},
					{ // the depth one
						.format = depthFormat,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
						.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
						.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
						.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					}
				};

				// SUBPASSES
				VkAttachmentReference colorAttachmentRef{};
				colorAttachmentRef.attachment = 0;
				colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				VkAttachmentReference depthAttachmentRef{};
				depthAttachmentRef.attachment = 1;
				depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				VkSubpassDescription subpass{};
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &colorAttachmentRef;
				subpass.pDepthStencilAttachment = &depthAttachmentRef;

				// DEPENDENCIES

				VkSubpassDependency dependency{};
				dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
				dependency.dstSubpass = 0;
				dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.srcAccessMask = 0;
				dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				// CREATE
				VkRenderPassCreateInfo renderPassInfo{};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
				renderPassInfo.attachmentCount = _countof(attachments);
				renderPassInfo.pAttachments = attachments;
				renderPassInfo.subpassCount = 1;
				renderPassInfo.pSubpasses = &subpass;
				renderPassInfo.dependencyCount = 1;
				renderPassInfo.pDependencies = &dependency;

				VulkanCheck(vkCreateRenderPass(Device, &renderPassInfo, nullptr, &SwapchainBuffer.RenderPass)); 
			}

			// Framebuffer Creation
			{
				for (VkImageView imageView : SwapchainBuffer.ImageViews)
				{
					VkImageView attachments[] = {
						imageView,
						SwapchainBuffer.DepthImageView
					};

					VkFramebufferCreateInfo framebufferInfo{};
					framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
					framebufferInfo.renderPass = SwapchainBuffer.RenderPass;
					framebufferInfo.attachmentCount = _countof(attachments);
					framebufferInfo.pAttachments = attachments;
					framebufferInfo.width = SwapchainExtent.width;
					framebufferInfo.height = SwapchainExtent.height;
					framebufferInfo.layers = 1;

					VkFramebuffer framebuffer;
					VulkanCheck(vkCreateFramebuffer(Device, &framebufferInfo, nullptr, &framebuffer));
					SwapchainBuffer.Framebuffers.push_back(framebuffer);
				}
			}
			
			// imgui set up
			if(ImGuiDescriptorPool == nullptr)
			{
				// Create Descriptor Pool
				{
					VkDescriptorPoolSize pool_sizes[] =
					{
						{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
						{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
						{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
						{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
						{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
						{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
						{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
						{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
						{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
						{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
						{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
					};
					VkDescriptorPoolCreateInfo pool_info = {};
					pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
					pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
					pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
					pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
					pool_info.pPoolSizes = pool_sizes;
					VulkanCheck(vkCreateDescriptorPool(Device, &pool_info, VK_NULL_HANDLE, &ImGuiDescriptorPool));
				}

				// Setup Platform/Renderer bindings
				ImGui_ImplVulkan_InitInfo init_info = {};
				init_info.Instance = Instance;
				init_info.PhysicalDevice = PhysicalDevice;
				init_info.Device = Device;
				init_info.QueueFamily = QueueFamily;
				init_info.Queue = Queue;
				init_info.PipelineCache = VK_NULL_HANDLE;
				init_info.DescriptorPool = ImGuiDescriptorPool;
				init_info.Allocator = VK_NULL_HANDLE;
				init_info.MinImageCount = (u32)SwapchainBuffer.Images.size();
				init_info.ImageCount = (u32)SwapchainBuffer.Images.size();
				init_info.CheckVkResultFn = nullptr;
				ImGui_ImplVulkan_Init(&init_info, SwapchainBuffer.RenderPass);

				{
					auto& cmd = NextCommandBuffer(false);

					ImGui_ImplVulkan_CreateFontsTexture(cmd.Buffer);

					Submit(false);
					SyncGpu();

					ImGui_ImplVulkan_DestroyFontUploadObjects();
				}
			}
		}

		void Gpu::ResizeTransferBuffer(u64 size)
		{
			// this function will always be called when allocating a buffer as a safety thing
			// but if the transfer buffer is equal to or greater than the reuqested size then no need to realloc
			if (TransferSize >= size)
				return;

			vkDestroyBuffer(Device, TransferBuffer, nullptr);
			vkFreeMemory(Device, TransferMemory, nullptr);

			TransferSize = size;

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = TransferSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VulkanCheck(vkCreateBuffer(Device, &bufferInfo, nullptr, &TransferBuffer));
			
			VkMemoryRequirements memReq;
			vkGetBufferMemoryRequirements(Device, TransferBuffer, &memReq);
			
			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memReq.size;
			allocInfo.memoryTypeIndex = findMemoryType(PhysicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VulkanCheck(vkAllocateMemory(Device, &allocInfo, nullptr, &TransferMemory));

			VulkanCheck(vkBindBufferMemory(Device, TransferBuffer, TransferMemory, 0));
		}

		VertexBuffer Gpu::CreateVertexBuffer(std::vector<std::pair<int, void*>>& vertexArrays, u32* indexArray, u32 indexSize)
		{
			VertexBuffer buffer;

			std::vector<u64> gpuSize;

			u64 size = 0;
			VkMemoryRequirements memReq = {};

			buffer.indexCount = indexSize / sizeof(u32);

			for (std::pair<int, void*>& array : vertexArrays)
			{
				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = array.first;
				bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VkBuffer b;
				VulkanCheck(vkCreateBuffer(Device, &bufferInfo, nullptr, &b));
				buffer.VertexBuffers.push_back(b);

				vkGetBufferMemoryRequirements(Device, b, &memReq);
				size += memReq.size;
				gpuSize.push_back(memReq.size);
			}

			{
				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = indexSize;
				bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VulkanCheck(vkCreateBuffer(Device, &bufferInfo, nullptr, &buffer.IndexBuffer));
				
				vkGetBufferMemoryRequirements(Device, buffer.IndexBuffer, &memReq);
				size += memReq.size;
			}

			{
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = size;
				allocInfo.memoryTypeIndex = findMemoryType(PhysicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				VulkanCheck(vkAllocateMemory(Device, &allocInfo, nullptr, &buffer.Memory));
			}

			{
				u64 offset = 0;
				// associate vertex buffers
				for (int i = 0; i < buffer.VertexBuffers.size(); i++)
				{
					VulkanCheck(vkBindBufferMemory(Device, buffer.VertexBuffers[i], buffer.Memory, offset));
					offset += gpuSize[i];
				}

				// associate index buffer
				VulkanCheck(vkBindBufferMemory(Device, buffer.IndexBuffer, buffer.Memory, offset));

			}

			{
				
				for (int i = 0; i < buffer.VertexBuffers.size(); i++)
				{
					u64 arraySize = vertexArrays[i].first;

					// transfer time
					ResizeTransferBuffer(arraySize);

					void* mmap;
					vkMapMemory(Device, TransferMemory, 0, arraySize, 0, &mmap);
					memcpy(mmap, vertexArrays[i].second, arraySize);
					vkUnmapMemory(Device, TransferMemory);

					auto& cmd = NextCommandBuffer(false);

					VkBufferCopy copyRegion{};
					copyRegion.size = arraySize;
					vkCmdCopyBuffer(cmd.Buffer, TransferBuffer, buffer.VertexBuffers[i], 1, &copyRegion);

					Submit(false);
					SyncGpu();
				}

				// index transfer time
				 ResizeTransferBuffer(indexSize);

				void* mmap;
				vkMapMemory(Device, TransferMemory, 0, indexSize, 0, &mmap);
				memcpy(mmap, indexArray, indexSize);
				vkUnmapMemory(Device, TransferMemory);

				auto& cmd = NextCommandBuffer(false);

				VkBufferCopy copyRegion{};
				copyRegion.size = indexSize;
				vkCmdCopyBuffer(cmd.Buffer, TransferBuffer, buffer.IndexBuffer, 1, &copyRegion);

				Submit(false);
				SyncGpu();

			}

			return buffer;

		}

		Shader Gpu::CreateShaderFromFile(ShaderLayout& layout, const std::string& vert, const std::string& frag)
		{
			ShaderProgram program;
			// create descriptor set layout
			std::vector<VkDescriptorSetLayoutBinding> bindings(layout.DescriptorSet.size());
			for (u32 i = 0; i < layout.DescriptorSet.size(); i++)
			{
				auto& descriptor = layout.DescriptorSet[i];
				if (!descriptor.Set)
				{
					abort();
				}

				VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

				switch (descriptor.Type)
				{
				case ShaderLayout::DescriptorType::Buffer:
					type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					break;
				case ShaderLayout::DescriptorType::Texture:
					type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					break;
				}

				bindings[i] = {
					.binding = i,
					.descriptorType = type,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_ALL,
					.pImmutableSamplers = nullptr
				};
			}

			VkDescriptorSetLayoutCreateInfo info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = (u32)bindings.size(),
				.pBindings = bindings.data()
			};
			vkCreateDescriptorSetLayout(Device, &info, nullptr, &program.Layout);

			// push constant
			VkPushConstantRange constantRange = {
				.stageFlags = VK_SHADER_STAGE_ALL,
				.offset = 0,
				.size = layout.ConstantSize,
			};

			VkPipelineLayoutCreateInfo pipelineLayoutInfo = { // TODO(caleb) : add shader layout
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1, // TODO(caleb) temp code
				.pSetLayouts = &program.Layout,
				.pushConstantRangeCount = layout.ConstantSize > 0, // Optional
				.pPushConstantRanges = &constantRange, // Optional
			};

			VulkanCheck(vkCreatePipelineLayout(Device, &pipelineLayoutInfo, nullptr, &program.PipelineLayout));

			std::vector<unsigned int> spirv;
			CompileShader(vert.c_str(), spirv, ShaderType::Vertex);

			VkShaderModuleCreateInfo shaderCreateInfo{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = spirv.size() * 4,
				.pCode = spirv.data(),
			};
			VulkanCheck(vkCreateShaderModule(Device, &shaderCreateInfo, nullptr, &program.VertexShader));

			spirv.clear();
			CompileShader(frag.c_str(), spirv, ShaderType::Fragment);
			shaderCreateInfo.codeSize = spirv.size() * 4;
			shaderCreateInfo.pCode = spirv.data();
			VulkanCheck(vkCreateShaderModule(Device, &shaderCreateInfo, nullptr, &program.FragmentShader));

			// make the desriptor pool sizes
			{
				std::map<VkDescriptorType, u32> sizes;
				for (auto& binding : bindings)
				{
					if (sizes.find(binding.descriptorType) == sizes.end())
						sizes.insert_or_assign(binding.descriptorType, 1);
					else
						sizes[binding.descriptorType] += 1;
				}

				for (auto& pair : sizes)
				{
					program.PoolAllocateInfo.push_back(VkDescriptorPoolSize {
							.type = pair.first,
							.descriptorCount = pair.second
						});
				}
			}

			Shader shader;
			shader.Index = (u32)Shaders.size();
			Shaders.push_back(program);

			return shader;
		}

		UniformSet Gpu::CreateUniformSet(Shader& shader, ShaderLayout& layout)
		{
			UniformSet set;

			u32 allocSize = 0;
			VkMemoryRequirements memReq = {};

			{
				set.Uniforms.resize(layout.DescriptorSet.size());

				int index = 0;
				for (auto& desc : layout.DescriptorSet)
				{
					if (desc.Type == ShaderLayout::DescriptorType::Texture)
					{
						set.Uniforms[index].Type = UniformSet::UniformType::Texture;
					}
					else if (desc.Type == ShaderLayout::DescriptorType::Buffer)
					{
						set.Uniforms[index].Type = UniformSet::UniformType::Buffer;
						for (int i = 0; i < FramesInFlight; i++)
						{
							VkBufferCreateInfo bufferInfo{};
							bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
							bufferInfo.size = desc.Size;
							bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
							bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

							VkBuffer b;
							VulkanCheck(vkCreateBuffer(Device, &bufferInfo, nullptr, &b));
							set.Uniforms[index].Buffers.push_back(b);

							vkGetBufferMemoryRequirements(Device, b, &memReq);
							set.Uniforms[index].BufferOffsets.push_back(allocSize);
							allocSize += (u32)memReq.size;
						}
					}
					index++;
				}
			}

			{
				if (allocSize > 0)
				{
					VkMemoryAllocateInfo allocInfo{};
					allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
					allocInfo.allocationSize = allocSize;
					allocInfo.memoryTypeIndex = findMemoryType(PhysicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

					VulkanCheck(vkAllocateMemory(Device, &allocInfo, nullptr, &set.Memory));
				}
			}

			{
				
				for (auto& uniform : set.Uniforms)
				{
					if(uniform.Type == UniformSet::UniformType::Buffer)
						// associate vertex buffers
						for (int i = 0; i < uniform.Buffers.size(); i++)
						{
							VulkanCheck(vkBindBufferMemory(Device, uniform.Buffers[i], set.Memory, uniform.BufferOffsets[i]));
						}
				}
			}

			{
				for (int i = 0; i < FramesInFlight; i++)
				{
					auto desc = AllocateDescriptor(shader);

					u32 index = 0;
					for (auto& uniform : set.Uniforms)
					{
						if (uniform.Type == UniformSet::UniformType::Buffer)
						{
							VkDescriptorBufferInfo bufferInfo{};
							bufferInfo.buffer = uniform.Buffers[i];
							bufferInfo.offset = 0;
							bufferInfo.range = VK_WHOLE_SIZE;

							VkWriteDescriptorSet descriptorWrite = {
								.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
								.pNext = nullptr,
								.dstSet = desc,
								.dstBinding = index,
								.dstArrayElement = 0,
								.descriptorCount = 1,
								.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								.pImageInfo = nullptr,
								.pBufferInfo = &bufferInfo,
								.pTexelBufferView = nullptr,
							};

							vkUpdateDescriptorSets(Device, 1, &descriptorWrite, 0, nullptr);
						}
						index++;
					}

					set.DescriptorSets.push_back(desc);
				}
			}

			return set;
		}

		Texture Gpu::CreateTexture(void* data, u32 width, u32 height)
		{
			Texture texture;
			VkFormat format					= VK_FORMAT_R8G8B8A8_SRGB;
			VkImageTiling tiling			= VK_IMAGE_TILING_LINEAR; 
			VkImageUsageFlags usage			= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VkImageAspectFlags aspectFlags	= VK_IMAGE_ASPECT_COLOR_BIT;
			
			VkMemoryRequirements memReq;
			// Create Image
			{
				VkImageCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				createInfo.pNext = nullptr;
				createInfo.flags = 0;
				createInfo.imageType = VK_IMAGE_TYPE_2D;
				createInfo.format = format;
				createInfo.extent.width = width;
				createInfo.extent.height = height;
				createInfo.extent.depth = 1;
				createInfo.mipLevels = 1;
				createInfo.arrayLayers = 1;
				createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				createInfo.tiling = tiling;
				createInfo.usage = usage;
				createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0;
				createInfo.pQueueFamilyIndices = nullptr;
				createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				VulkanCheck(vkCreateImage(Device, &createInfo, nullptr, &texture.Image));

				vkGetImageMemoryRequirements(Device, texture.Image, &memReq);
			}

			// Allocate GPU Memory
			{
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memReq.size	;
				allocInfo.memoryTypeIndex = findMemoryType(PhysicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

				VulkanCheck(vkAllocateMemory(Device, &allocInfo, nullptr, &texture.Memory));

				VulkanCheck(vkBindImageMemory(Device, texture.Image, texture.Memory, 0));
			}

			// Copy image into transfer buffer
			{
				VkDeviceSize imageSize = width * height * 4;
				ResizeTransferBuffer(memReq.size);
				void* mmap;
				vkMapMemory(Device, TransferMemory, 0, memReq.size, 0, &mmap);
				memcpy(mmap, data, static_cast<size_t>(imageSize));
				vkUnmapMemory(Device, TransferMemory);
			}

			// Copy buffer to gpu memory
			{
				auto& cmd = NextCommandBuffer(false);

				VkImageMemoryBarrier barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = texture.Image,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					}
				};
				
				vkCmdPipelineBarrier(cmd.Buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

				VkBufferImageCopy region = {
					.bufferOffset = 0,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.imageOffset = { 0, 0, 0 },
					.imageExtent = {
						width,
						height,
						1
					},
				};
				
				vkCmdCopyBufferToImage(cmd.Buffer, TransferBuffer, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = texture.Image,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.levelCount = 1,
						.layerCount = 1
					}
				};
				
				vkCmdPipelineBarrier(cmd.Buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

				Submit(false);
				SyncGpu();
			}

			// Create image view
			{
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = texture.Image;
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = format;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.subresourceRange.aspectMask = aspectFlags;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				VulkanCheck(vkCreateImageView(Device, &createInfo, nullptr, &texture.ImageView));
			}

			// Create sampler
			{
				VkSamplerCreateInfo samplerInfo = {
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.magFilter = VK_FILTER_LINEAR,
					.minFilter = VK_FILTER_LINEAR,
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
					.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.mipLodBias = 0.0f,
					.anisotropyEnable = VK_TRUE,
					.maxAnisotropy = 16.0f,
					.compareEnable = VK_FALSE,
					.compareOp = VK_COMPARE_OP_ALWAYS,
					.minLod = 0.0f,
					.maxLod = 0.0f,
					.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
					.unnormalizedCoordinates = VK_FALSE,
				};
				VulkanCheck(vkCreateSampler(Device, &samplerInfo, nullptr, &texture.Sampler));
			}
			return texture;
		}

		RenderBuffer& Gpu::GetSwapchainBuffer()
		{
			return SwapchainBuffer;
		}

		VkPipeline Gpu::GetPipeline(PipelineOptions& options)
		{
			u64 key = 0;

			if (PipelineCache.find(key) == PipelineCache.end())
			{
				auto& program = Shaders[options.Shader.Index];

				VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
				vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
				vertShaderStageInfo.module = program.VertexShader;
				vertShaderStageInfo.pName = "main";

				VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
				fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				fragShaderStageInfo.module = program.FragmentShader;
				fragShaderStageInfo.pName = "main";

				VkPipelineShaderStageCreateInfo shaderStages[] = {
					vertShaderStageInfo, fragShaderStageInfo
				};
				
				VkPipelineInputAssemblyStateCreateInfo iaCreateInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
					.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					.primitiveRestartEnable = VK_FALSE
				};
				
				VkViewport viewport = 
				{
					.x = 0.0f,
					.y = 0.0f,
					.width = 1600.0f,
					.height = 900.0f,
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				};

				VkRect2D scissor = 
				{
					.offset = { 0, 0 },
					.extent = SwapchainExtent
				};

				VkPipelineViewportStateCreateInfo viewportCreateInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
					.viewportCount = 1,
					.pViewports = &viewport,
					.scissorCount = 1,
					.pScissors = &scissor
				};

				VkPipelineRasterizationStateCreateInfo rasterisationCreateInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.depthClampEnable = VK_FALSE,
					.rasterizerDiscardEnable = VK_FALSE,
					.polygonMode = VK_POLYGON_MODE_FILL,
					.cullMode = VK_CULL_MODE_BACK_BIT,
					.frontFace = VK_FRONT_FACE_CLOCKWISE,
					.depthBiasEnable = VK_FALSE,
					.depthBiasConstantFactor = 0.0f, // Optional
					.depthBiasClamp = 0.0f, // Optional
					.depthBiasSlopeFactor = 0.0f, // Optional
					.lineWidth = 1.0f,
				};

				VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
					.pNext = nullptr,
					.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
					.sampleShadingEnable = VK_FALSE,
					.minSampleShading = 1.0f, // Optional
					.pSampleMask = nullptr, // Optional
					.alphaToCoverageEnable = VK_FALSE, // Optional
					.alphaToOneEnable = VK_FALSE, // Optional
				};

				VkPipelineDepthStencilStateCreateInfo depthStenciCreateInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.depthTestEnable = VK_TRUE,
					.depthWriteEnable = VK_TRUE,
					.depthCompareOp = VK_COMPARE_OP_LESS,
					.depthBoundsTestEnable = VK_FALSE,
					.stencilTestEnable = VK_FALSE,
					.front = {}, // Optional
					.back = {}, // Optional
					.minDepthBounds = 0.0f, // Optional
					.maxDepthBounds = 1.0f, // Optional
				};

				VkPipelineColorBlendAttachmentState colorBlendState =
				{
					.blendEnable = VK_FALSE,
					.srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
					.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
					.colorBlendOp = VK_BLEND_OP_ADD, // Optional
					.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
					.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
					.alphaBlendOp = VK_BLEND_OP_ADD, // Optional
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
				};

				VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo =
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.logicOpEnable = VK_FALSE,
					.logicOp = VK_LOGIC_OP_COPY, // Optional
					.attachmentCount = 1,
					.pAttachments = &colorBlendState,
					.blendConstants = {0, 0, 0, 0}
				};

				VkDynamicState dynamicStates[2];
				dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
				dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

				VkPipelineDynamicStateCreateInfo dynamicState = {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
					.dynamicStateCount = 2,
					.pDynamicStates = dynamicStates
				};

				std::vector<VkVertexInputBindingDescription> bindingDescriptions;
				std::vector<VkVertexInputAttributeDescription> bindingAttributes;

				bindingDescriptions.clear();
				bindingDescriptions.resize(3);
				// vertex position buffer
				bindingDescriptions[0].binding = 0;
				bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				bindingDescriptions[0].stride = sizeof(float) * 3;
				// uv buffer
				bindingDescriptions[1].binding = 1;
				bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				bindingDescriptions[1].stride = sizeof(float) * 2;
				// normal buffer
				bindingDescriptions[2].binding = 2;
				bindingDescriptions[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				bindingDescriptions[2].stride = sizeof(float) * 3;
				//// normal buffer
				//bindingDescriptions[3].binding = 3;
				//bindingDescriptions[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				//bindingDescriptions[3].stride = sizeof(float) * 3;
				//// normal buffer
				//bindingDescriptions[4].binding = 4;
				//bindingDescriptions[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				//bindingDescriptions[4].stride = sizeof(float) * 3;

				bindingAttributes.clear();
				bindingAttributes.resize(3);
				// vertex position binding
				bindingAttributes[0].binding = 0;
				bindingAttributes[0].location = 0;
				bindingAttributes[0].offset = 0;
				bindingAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				// uv binding
				bindingAttributes[1].binding = 1;
				bindingAttributes[1].location = 1;
				bindingAttributes[1].offset = 0;
				bindingAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
				// normal binding
				bindingAttributes[2].binding = 2;
				bindingAttributes[2].location = 2;
				bindingAttributes[2].offset = 0;
				bindingAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
				//// tangent binding
				//bindingAttributes[3].binding = 3;
				//bindingAttributes[3].location = 3;
				//bindingAttributes[3].offset = 0;
				//bindingAttributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
				//// bitangent binding
				//bindingAttributes[4].binding = 4;
				//bindingAttributes[4].location = 4;
				//bindingAttributes[4].offset = 0;
				//bindingAttributes[4].format = VK_FORMAT_R32G32B32_SFLOAT;

				VkPipelineVertexInputStateCreateInfo vertexInputInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
					.vertexBindingDescriptionCount = (u32)bindingDescriptions.size(),
					.pVertexBindingDescriptions = bindingDescriptions.data(),
					.vertexAttributeDescriptionCount = (u32)bindingAttributes.size(),
					.pVertexAttributeDescriptions = bindingAttributes.data(),
				};

				VkPushConstantRange range = {
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = 16 * sizeof(float),
				};

				VkGraphicsPipelineCreateInfo createInfo
				{
					.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.stageCount = 2,
					.pStages = shaderStages,
					.pVertexInputState = &vertexInputInfo,
					.pInputAssemblyState = &iaCreateInfo,
					.pViewportState = &viewportCreateInfo,
					.pRasterizationState = &rasterisationCreateInfo,
					.pMultisampleState = &multisampleCreateInfo,
					.pDepthStencilState = &depthStenciCreateInfo,
					.pColorBlendState = &colorBlendCreateInfo,
					.pDynamicState = &dynamicState,
					.layout = program.PipelineLayout,
					.renderPass = SwapchainBuffer.RenderPass,
					.subpass = 0,
					.basePipelineHandle = VK_NULL_HANDLE,
					.basePipelineIndex = -1

				};

				VkPipeline pipeline;
				vkCreateGraphicsPipelines(Device, nullptr, 1, &createInfo, nullptr, &pipeline);
				PipelineCache[key] = pipeline;
			}
			return PipelineCache[key];
		}

		VkPipelineLayout Gpu::GetPipelineLayout(Shader& shader)
		{
			return Shaders[shader.Index].PipelineLayout;
		}

		void Gpu::ResetPools()
		{
			for(auto& shader : Shaders)
				for (auto& pool : shader.DescriptorPools[FrameIterator])
				{
					vkResetDescriptorPool(Device, pool.Pool, 0);
					pool.Count = 0;
				}
		}

		VkDescriptorSet Gpu::AllocateDescriptor(Shader& s)
		{
			const int PoolSize = 10;
			
			auto& shader = Shaders[s.Index];
			int index = 0;
			auto& pools = shader.DescriptorPools[FrameIterator];
			auto pool = pools.begin();
			bool found = false;
			while (pool != pools.end())
			{
				if (pool->Count < PoolSize)
				{
					index = pool->Count;
					pool->Count++;
					break;
				}
				pool++;
			}

			if (pool == pools.end())
			{
				auto sizes = shader.PoolAllocateInfo;
				for (auto& size : sizes)
					size.descriptorCount *= PoolSize;
				VkDescriptorPool newPool;
				VkDescriptorPoolCreateInfo createInfo = 
				{
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.maxSets = PoolSize,
					.poolSizeCount = (u32)sizes.size(),
					.pPoolSizes = sizes.data()
				};
				VulkanCheck( vkCreateDescriptorPool(Device, &createInfo, nullptr, &newPool) );

				pools.push_back(DescriptorPool{
					.Pool = newPool,
					.Count = 0
				});
				pool = pools.end() - 1;
			}

			VkDescriptorSetAllocateInfo allocInfo = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = pool->Pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &shader.Layout
			};
			VkDescriptorSet descriptorSet;
			vkAllocateDescriptorSets(Device, &allocInfo, &descriptorSet);
			return descriptorSet;
		}

		CommandBuffer& Gpu::NextCommandBuffer(b8 acquireImage)
		{
			//printf("waiting on a fence... ");
			VulkanCheck(vkWaitForFences(Device, 1, &CommandBuffers[FrameIterator].Fence, VK_TRUE, UINT64_MAX));
			//printf("done\n");

			if (acquireImage)
			{
				VulkanCheck(vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ImageAvailableSemaphores[FrameIterator], VK_NULL_HANDLE, &ImageIndex));
			}
			
			VulkanCheck(vkResetFences(Device, 1, &CommandBuffers[FrameIterator].Fence));
			VulkanCheck(vkResetCommandBuffer(CommandBuffers[FrameIterator].Buffer, 0));

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0; // Optional
			beginInfo.pInheritanceInfo = nullptr; // Optional

			VulkanCheck(vkBeginCommandBuffer(CommandBuffers[FrameIterator].Buffer, &beginInfo));

			CommandBuffers[FrameIterator].CurrentImage = ImageIndex;
			//ResetPools();
			return CommandBuffers[FrameIterator];
		}

		void Gpu::Submit(b8 present)
		{
			VulkanCheck(vkEndCommandBuffer(CommandBuffers[FrameIterator].Buffer));

			if (present)
			{
				VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = &ImageAvailableSemaphores[FrameIterator];
				submitInfo.pWaitDstStageMask = waitStages;
				submitInfo.commandBufferCount = _countof(waitStages);
				submitInfo.pCommandBuffers = &CommandBuffers[FrameIterator].Buffer;
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = &RenderFinishedSemaphores[FrameIterator];

				VulkanCheck(vkQueueSubmit(Queue, 1, &submitInfo, CommandBuffers[FrameIterator].Fence));

				VkPresentInfoKHR presentInfo{};
				presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				presentInfo.waitSemaphoreCount = 1;
				presentInfo.pWaitSemaphores = &RenderFinishedSemaphores[FrameIterator];
				presentInfo.swapchainCount = 1;
				presentInfo.pSwapchains = &Swapchain;
				presentInfo.pImageIndices = &ImageIndex;
				presentInfo.pResults = nullptr;

				vkQueuePresentKHR(Queue, &presentInfo);
			}
			else
			{
				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.waitSemaphoreCount = 0;
				submitInfo.pWaitSemaphores = nullptr;
				submitInfo.pWaitDstStageMask = nullptr;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &CommandBuffers[FrameIterator].Buffer;
				submitInfo.signalSemaphoreCount = 0;
				submitInfo.pSignalSemaphores = nullptr;

				VulkanCheck(vkQueueSubmit(Queue, 1, &submitInfo, CommandBuffers[FrameIterator].Fence));

				vkWaitForFences(Device, 1, &CommandBuffers[FrameIterator].Fence, VK_TRUE, UINT64_MAX);
			}

			FrameIterator = (FrameIterator + 1) % FramesInFlight;
		}

		void CommandBuffer::StartRenderPass(RenderBuffer& buffer)
		{
			VkClearValue clearColors[2];
			clearColors[0].color = { 0.39f, 0.582f, 0.926f, 1.0f };
			clearColors[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = buffer.RenderPass;
			renderPassInfo.framebuffer = buffer.Framebuffers[CurrentImage];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = buffer.RenderExtent;
			renderPassInfo.clearValueCount = 2;
			renderPassInfo.pClearValues = clearColors;

			vkCmdBeginRenderPass(Buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {
				.x = 0,
				.y = 0,
				.width = (f32)buffer.RenderExtent.width,
				.height = (f32)buffer.RenderExtent.height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f,
			};
			vkCmdSetViewport(Buffer, 0, 1, &viewport);
			
			VkRect2D scissor;
			scissor.offset = {};
			scissor.extent = buffer.RenderExtent;
			vkCmdSetScissor(Buffer, 0, 1, &scissor);

			Current.DescriptorSet = nullptr;
		}

		void CommandBuffer::EndRenderPass(RenderBuffer& buffer)
		{
			vkCmdEndRenderPass(Buffer);
		}
		
		void CommandBuffer::RenderImGui()
		{
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Buffer);
		}

		void CommandBuffer::BindUniformSet(u32 index, UniformSet& buffer)
		{
			auto layout = Gpu::GetPipelineLayout(Current.Shader);
			vkCmdBindDescriptorSets(Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &buffer.DescriptorSets[Index], 0, nullptr);
		}

		void CommandBuffer::BindShader(Shader& shader)
		{
			PipelineOptions options = {
				.Shader = shader
			};
			auto pipeline = Gpu::GetPipeline(options);
			vkCmdBindPipeline(Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			Current.Shader = shader;
		}
		
		void CommandBuffer::Draw(VertexBuffer& buffer)
		{
			// get the descriptor set
			VkDeviceSize offsets[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			vkCmdBindVertexBuffers(Buffer, 0, (u32)buffer.VertexBuffers.size(), buffer.VertexBuffers.data(), offsets);
			vkCmdBindIndexBuffer(Buffer, buffer.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(Buffer, buffer.indexCount, 1, 0, 0, 0);
		}

		size_t PipelineOptions::GetHash()
		{
			return 0;
		}

		void ShaderLayout::assure(u32 index)
		{
			u32 size = index + 1;
			if(size > DescriptorSet.size())
				DescriptorSet.resize(size);
		}
	}
}