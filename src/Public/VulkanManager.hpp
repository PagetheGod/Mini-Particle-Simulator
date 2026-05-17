//
// Created by YWvin on 2026/3/29.
//

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <cstdint>
#include <string>
#include "Commons.hpp"
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
// For debug/validation layer
static constexpr bool ENABLE_VALIDATION_LAYERS = true;
const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};


/* Helper: Find queue families for a physical device
/* Returns true if the device has all queue families we need.
/* Fills in the family indices in the context. */
struct QueueFamilyIndices
{
	uint32_t GraphicsFamily = UINT32_MAX;
	uint32_t PresentFamily = UINT32_MAX;
	uint32_t ComputeFamily = UINT32_MAX;
	bool IsComplete() const
	{
		return GraphicsFamily != UINT32_MAX && PresentFamily != UINT32_MAX && ComputeFamily != UINT32_MAX;
	}
};

struct PushConstantType
{
	glm::mat4 ViewProjection;
	glm::vec3 CameraRight;
	float Padding1;
	glm::vec3 CameraUp;
	float Padding2;
};

// A helper struct that wraps an allocated vulkan buffer
// With additional information about its memory and size
struct AllocatedVkBuffer
{
	VkBuffer VulkanBuffer = nullptr;
	VkDeviceMemory VulkanDeviceMemory = nullptr;
	VkDeviceSize VulkanBufferSize = 0;
};

struct ParticleInstanceBuffers
{
	// SoA buffers for particle instance data
	// One for each particle attributes, and same order as they are on the CPU side
	AllocatedVkBuffer Px;
	AllocatedVkBuffer Py;
	AllocatedVkBuffer Pz;
	AllocatedVkBuffer R;
	AllocatedVkBuffer G;
	AllocatedVkBuffer B;
	AllocatedVkBuffer Size;
	AllocatedVkBuffer LifeTime;
	AllocatedVkBuffer MaxLifeTime;

	/*
	 * CPU pointers to the buffers above, one per buffer
	 * These are persistently mapped GPU VRAM, and mapped once at init
	 * They are returned by vkMapMemory
	 */
	void* MappedPtr[9] = {};
	/*
	 * The descriptor set bound to the graphic pipelines for this frame
	 * This points to the 9 buffers above. Again, allocated once at init
	 * These are bound per-frame in record frame command buffer
	 */
	VkDescriptorSet DescriptorSet = nullptr;
};

/* Offscreen render target for supersampled particle rendering
 * Particles rendered directly to swapchain images.
 * Now particles render to this 2560×1440 image, then get blitted to
 * the viewport region of the swapchain.
 */
struct OffscreenTarget
{
	VkImage VkImage = nullptr; // Similar to DX11Texture2D
	VkDeviceMemory VkMemory = nullptr;
	VkImageView VkImageView = nullptr;
	VkRenderPass RenderPass = nullptr;
	VkFramebuffer Framebuffer = nullptr;
	uint32_t Width  = 0;
	uint32_t Height = 0;
	VkFormat Format = VK_FORMAT_UNDEFINED;
};

// Vulkan context that controls vulkan states, these are vulkan objects
// Bundled into a fat struct so they get cleaned up easily
// Emphasis on easy cleanup, this huge bag of evils would hurt my sanity if I don't do bundling like this
struct VulkanContext
{
	//Core functionalities
	VkInstance VulkanInstance = nullptr;
	VkDebugUtilsMessengerEXT VulkanDebugMessenger = nullptr;
	VkSurfaceKHR VulkanSurface = nullptr;//The extension(non-core vk) that allows us to actually display stuffs
	VkPhysicalDevice VulkanPhysicalDevice = nullptr;
	VkDevice VulkanDevice = nullptr;

	//Queue Handles
	VkQueue GraphicsQueue = nullptr;// Sumbit draw commands here
	VkQueue PresentQueue = nullptr;// Presentation queue
	VkQueue ComputeQueue = nullptr;// Optional queue to dispatch compute shaders

	// Queue family indices
	uint32_t GraphicsFamily = UINT32_MAX;
	uint32_t PresentFamily = UINT32_MAX;
	uint32_t ComputeFamily = UINT32_MAX;

	// Swapchain
	VkSwapchainKHR SwapChain = nullptr;
	VkFormat SwapChainFormat = VK_FORMAT_UNDEFINED;// Same stuffs as the DX11 formats(DXGI_FORMAT_XXX)
	VkExtent2D SwapChainExtent = { 0, 0 };
	uint32_t MinImageCount = 0;
	std::vector<VkImage> SwapChainImages;
	std::vector<VkImageView> SwapChainImageViews;// Yes it's view, does what it says

	// Offscreen render target
    // This is the 2560×1440 image particles render to.
	OffscreenTarget OffScreen;


	/* Render pass & framebuffers
	 * Offscreen.render_pass (Pass A): particles → offscreen image
	 * loadOp=CLEAR, finalLayout=TRANSFER_SRC_OPTIMAL
	 * Swapchain_render_pass (Pass B): ImGui → swapchain image
	 * loadOp=LOAD, finalLayout=PRESENT_SRC_KHR
	 */
	VkRenderPass SwapChainRenderPass = nullptr;
	std::vector<VkFramebuffer> FrameBuffers;

	//Pipeline
	VkPipelineLayout GraphicsPipelineLayout = nullptr;
	VkPipeline GraphicsPipeline = nullptr;
	VkPipelineLayout ComputePipelineLayout = nullptr;
	VkPipeline ComputePipeline = nullptr;

	// Command pools and buffers
	VkCommandPool CommandPool = nullptr;
	std::vector<VkCommandBuffer> CommandBuffers;

	// Synchronization
	std::vector<VkSemaphore> ImageAvailableSemaphores;
	std::vector<VkSemaphore> RenderFinishedSemaphores;
	std::vector<VkFence> InFlightFence;

	// Descriptor sets for compute
	VkDescriptorSetLayout DescriptorSetLayout = nullptr;
	VkDescriptorPool      DescriptorPool = nullptr;
	std::vector<VkDescriptorSet> DescriptorSets;

	// SDL window reference
	SDL_Window* Window = nullptr;

	// Maximum frames that can be in-flight simultaneously.
	// 2 means double-buffering of command submission (not the swapchain).
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
	uint32_t CurrentFrame = 0;

	// New particle SoA instance buffers
	ParticleInstanceBuffers ParticleInstanceBufferArray[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSetLayout ParticleInstanceDescriptorSetLayout = nullptr;
	VkDescriptorPool ParticleInstanceDescriptorPool = nullptr;
};


// This class encapsulates all Vulkan-specific behaviors:
// Initializing vulkan, loading shaders, issuing draw calls, etc.
class VulkanManager
{
public:
    VulkanManager();
    VulkanManager(const VulkanManager&) = delete;
    VulkanManager& operator=(const VulkanManager&) = delete;
    VulkanManager(VulkanManager&& ) = delete;
    VulkanManager& operator=(VulkanManager&&) = delete;
    ~VulkanManager();

    // Actual work functions
    bool Initialize(SDL_Window* InWindow);
	void DrawFrame(uint32_t CurrentFrame, uint32_t InstanceCount);

public:
	// Setters and getters
	[[nodiscard]] VulkanContext* GetVulkanContext()
	{
		return &m_VulkanContext;
	}

private:
	// Create vulkan instance with all the validation layers and SDL3 extensions
	bool CreateInstance();

	// Set up debug messenger callback(validation layer output)
	bool SetupDebugMessenger();
	bool CreateSurface();

	// Check blit format support
	bool CheckBlitSupport(VkPhysicalDevice Device, VkFormat Format);

	// Function rate physical devices(GPUs), this helps us pick the best one to render
	uint32_t RatePhysicalDevices(VkPhysicalDevice Device, VkSurfaceKHR& Surface);
	bool PickPhysicalDevice();

	// Vulkan initialization chain, words cannot describe how much pain this list is causing me
	// Khronos and Microsoft, come on. The world had moved on from GTX 960 and 1060, you should too
	// Just copy CUDA's programming model and make a DX13/Vulkan 2.0 that's less painful to work with
	bool CreateLogicalDevice();
	bool CreateSwapChain();
	bool CreateOffscreenTarget(uint32_t Width, uint32_t Height);
	bool CreateOffScreenRenderPass();
	bool CreateSwapChainRenderPass();
	bool CreateFrameBuffers();
	bool CreateGraphicsPipeline();
	bool CreateCommandPool();
	bool AllocateCommandBuffers();
	bool CreateParticleInstanceBuffers(uint32_t NumMaxParticles);
	/*
	 * We need three descriptor objects to actually wire the storage buffers to the shaders:
	 * 1. Descriptor set layout, this describes what bindings the shader should expect(0 - 8, 9 buffers in set 0)
	 * 2. Descriptor pool, this is where the descriptor sets are allocated from. Sized by max frames in flight, each
	 * containing 9 storage buffer descriptors
	 * 3. One descriptor set per frame in flight, each pointing at the frame's buffers
	 */
	bool CreateParticleDescriptorLayout();
	bool CreateParticleDescriptorPoolsAndSets();
	AllocatedVkBuffer CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties);
	void GPUCopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size);
	AllocatedVkBuffer CreateBufferWithData(const void* Data, VkDeviceSize Size, VkBufferUsageFlags Usage);
	void DestroyBuffer(AllocatedVkBuffer& VulkanBuffer);
	bool CreateSyncObjects();
	void CleanUpContext();

	//Cleanups
	void DestroyOffscreenTarget();
	void DestorySwapChain();
	void DestoryCommandPool();
	void DestorySyncObjects();
	void DestroyParticleInstanceBuffers();

	// Helpers
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR Surface);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice Device);
	bool CheckValidationLayers();
	uint32_t FindMemoryType(VkPhysicalDevice PhysicalDevice, uint32_t TypeFilter, VkMemoryPropertyFlags Properties);
	bool ReadCompiledShader(const std::string &ShaderBinaryPath, std::vector<char>& OutShaderBuffer);
	bool CreateShaderModule(const std::vector<char>& InShaderBuffer, VkShaderModule& OutShaderModule);

    void RecordFrameCommandBuffer(VkCommandBuffer CommandBuffer, uint32_t CurrentFrame, uint32_t ImageIndex, uint32_t InstanceCount, VkBuffer VertexBuffer,
                                  const PushConstantType &PushConstantData, const Commons::Layout::ViewportRect& Viewport);

private:
	VulkanContext m_VulkanContext;
	// Device extensions we require. Every device must support these.
	// VK_KHR_SWAPCHAIN_EXTENSION_NAME is needed to present images to the screen.
	const std::vector<const char*> DEVICE_EXTENSIONS = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};


static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type, const VkDebugUtilsMessengerCallbackDataEXT* CallBackData,
	void* UserData);
