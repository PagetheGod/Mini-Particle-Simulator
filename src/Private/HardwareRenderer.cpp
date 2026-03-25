#include "HardwareRenderer.hpp"

#include <algorithm>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstring>  // for strcmp
#include <limits>
#include <iostream>
#include <format>
#include <set>


HardwareRenderer::HardwareRenderer() {
}

HardwareRenderer::~HardwareRenderer() {
}

bool HardwareRenderer::Initialize() {
}



bool HardwareRenderer::CreateInstance(VulkanContext& Context)
{
    // Step 1: Check validation layer support
    if (ENABLE_VALIDATION_LAYERS && !CheckValidationLayers())
    {
        std::cerr << "WARNING: Validation layers requested but not available.\n" <<
            "Install the Vulkan SDK to get them. Continuing without validation.\n";
    }

    // Step 2: Fill in the application info
    // This struct is technically optional but good practice. Some drivers
    // use the engine name/version for driver-specific optimizations.
    VkApplicationInfo AppInfo{};
    AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    AppInfo.pApplicationName = "Mini Particle Simulator";
    AppInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 1);
    AppInfo.pEngineName = "Custom";
    AppInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    AppInfo.apiVersion = VK_API_VERSION_1_4;

    // Step 3: Gather required extensions
    // SDL3 tells us which extensions it needs for surface creation.
    // This is the bridge from SDL3 Tutorial Module 4.
    uint32_t SDLExtensionCount = 0;
    // EVIL SYNTAX WARNING - this is an array of C-style strings, in the form of a const char**
    // And you cannot change where this pointer points to
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);
    if (!SDLExtensions)
    {
        std::cerr << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError() << '\n';
        return false;
    }
    // Start with SDL's required extensions
    std::vector<const char*> Extensions(SDLExtensions, SDLExtensions + SDLExtensionCount);

    // Add the debug utils extension if we're using validation layers.
    // This extension provides the VkDebugUtilsMessengerEXT object that
    // receives validation messages.
    if (ENABLE_VALIDATION_LAYERS)
    {
        Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Print what we're enabling (helpful for debugging setup issues)
    std::cout << "Vulkan instance extensions: " << Extensions.size() << '\n';
    for (const char* Ext : Extensions)
    {
        std::cout << "  - " << Ext << '\n';
    }
    // Step 4: Fill in the instance create info
    // this is a bit similar to create device through factory in DX11
    VkInstanceCreateInfo CreateInfo{};
    CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    CreateInfo.pApplicationInfo = &AppInfo;
    CreateInfo.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
    CreateInfo.ppEnabledExtensionNames = Extensions.data();
    // Enable validation layers if requested
    // The pNext chain below also hooks the debug messenger into instance
    // creation/destruction, so validation errors during those calls are caught.
    VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo{};
    if (ENABLE_VALIDATION_LAYERS)
    {
        CreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        CreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        // Set up a temporary debug messenger for instance creation.
        // Without this, errors during vkCreateInstance itself wouldn't
        // be caught by the debug messenger (because it doesn't exist yet).
        DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        DebugCreateInfo.pfnUserCallback = DebugCallback;
        // Chain the debug messenger into the instance creation
        CreateInfo.pNext = &DebugCreateInfo;
    }
    else
    {
        CreateInfo.enabledLayerCount = 0;
        CreateInfo.ppEnabledLayerNames = nullptr;
    }
    // Step 5: Create the instance
    const VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &Context.VulkanInstance);

    if (Result == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        std::cerr << "vkCreateInstance failed: VK_ERROR_INCOMPATIBLE_DRIVER\n" << "Your GPU driver does not support Vulkan 1.4.\n"
            << "Run 'vulkaninfo --summary' to check your Vulkan version.\n";
        return false;
    }

    if (Result != VK_SUCCESS)
    {
        std::cerr << "vkCreateInstance failed: VkResult = " << Result << '\n';
        return false;
    }

    std::cout << "Vulkan instance created successfully!" << '\n';

	return true;
}

/* Setup Debug Messenger
 * The debug messenger is a Vulkan object that receives validation messages.
 * Unlike the temporary one we chained into instance creation, this one
 * persists for the lifetime of the instance.
 *
 * Important: vkCreateDebugUtilsMessengerEXT is an extension function.
 * It's not part of the Vulkan core, so we can't call it directly.
 * We need to look it up via vkGetInstanceProcAddr. */

bool HardwareRenderer::SetupDebugMessenger(VulkanContext& Context)
{
    if constexpr (!ENABLE_VALIDATION_LAYERS)
    {
        return true;
    }

    VkDebugUtilsMessengerCreateInfoEXT CreateInfo{};
    CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    CreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    CreateInfo.pfnUserCallback = DebugCallback;
    CreateInfo.pUserData = nullptr;

    // Look up the extension function.
    // vkGetInstanceProcAddr returns a generic function pointer that we
    // cast to the specific function type. This is how all Vulkan extension
    // functions are loaded.
    auto Func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Context.VulkanInstance,
        "vkCreateDebugUtilsMessengerEXT");
    if (!Func)
    {
        std::cerr << "Could not load vkCreateDebugUtilsMessengerEXT" << '\n';
        return false;
    }

    VkResult Result = Func(Context.VulkanInstance, &CreateInfo, nullptr, &Context.VulkanDebugMessenger);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to set up debug messenger: " << Result << '\n';
        return false;
    }

    std::cout << "Vulkan debug messenger created." << '\n';
    return true;
}

bool HardwareRenderer::CreateSurface(VulkanContext& Context)
{
    // SDL3 creates the platform-specific Vulkan surface for us.
    // On Windows, this creates a VkWin32SurfaceKHR internally.
    // On Linux, it creates a VkXcbSurfaceKHR or VkWaylandSurfaceKHR.
    // We never need to know which — SDL3 handles it.
    //
    // The third parameter is the Vulkan memory allocator (nullptr = default).
    if (!SDL_Vulkan_CreateSurface(Context.Window, Context.VulkanInstance, nullptr, &Context.VulkanSurface))
    {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << '\n';
        return false;
    }
    std::cout << "Vulkan surface created from SDL3 window." << '\n';
    return true;
}

bool HardwareRenderer::CheckBlitSupport(VkPhysicalDevice Device, VkFormat Format)
{
    VkFormatProperties Props;
    vkGetPhysicalDeviceFormatProperties(Device, Format, &Props);

    // We need the swapchain format to support being a blit destination
    // (we blit the offscreen image to the swapchain)
    // and the offscreen format to support being a blit source.
    const bool IsDestSupported = (Props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) != 0;
    const bool IsSrcSupported = (Props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0;
    if (!IsDestSupported || !IsSrcSupported)
    {
        const std::string Warning = std::format("Warning, {} is missing blit support (src = {}, dest = {})",
            static_cast<int>(Format), IsSrcSupported, IsDestSupported);
        std::cerr << Warning << '\n';
    }

    return IsDestSupported && IsSrcSupported;
}

uint32_t HardwareRenderer::RatePhysicalDevices(VkPhysicalDevice Device, VkSurfaceKHR&Surface)
{
    QueueFamilyIndices Indices = FindQueueFamilies(Device, Surface);
    if (!Indices.IsComplete())
    {
        return 0;
    }
    if (!CheckDeviceExtensionSupport(Device))
    {
        return 0;
    }

    uint32_t FormatCount = 0;
    uint32_t PresentModeCount = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, nullptr);
    if (FormatCount == 0 || PresentModeCount == 0)
    {
        return 0;
    }
    VkPhysicalDeviceProperties Props;
    vkGetPhysicalDeviceProperties(Device, &Props);

    uint32_t Score = 0;
    if (Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        Score += 100;
    }
    else if (Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    {
        Score += 10;
    }
    Score += Props.limits.maxImageDimension2D;

    return Score;
}

bool HardwareRenderer::PickPhysicalDevice(VulkanContext &Context)
{
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(Context.VulkanInstance, &DeviceCount, nullptr);
    if (DeviceCount == 0)
    {
        std::cerr << "No Vulkan devices found!" << '\n';
        return false;
    }

    // Enumerate through all of the devices and pick the best
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    vkEnumeratePhysicalDevices(Context.VulkanInstance, &DeviceCount, Devices.data());

    uint32_t BestScore = 0;
    VkPhysicalDevice BestDevice = nullptr;

    for (const VkPhysicalDevice& Device : Devices)
    {
        VkPhysicalDeviceProperties Props;
        vkGetPhysicalDeviceProperties(Device, &Props);
        uint32_t Score = RatePhysicalDevices(Device, Context.VulkanSurface);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestDevice = Device;
        }
    }
    if (!BestDevice)
    {
        std::cerr << "No suitable Vulkan device found!" << '\n';
        return false;
    }

    Context.VulkanPhysicalDevice = BestDevice;
    QueueFamilyIndices Indices = FindQueueFamilies(BestDevice, Context.VulkanSurface);
    Context.GraphicsFamily = Indices.GraphicsFamily;
    Context.PresentFamily = Indices.PresentFamily;
    Context.ComputeFamily = Indices.ComputeFamily;

    /* Check blit support for B8G8R8A8_SRGB.
     * Note: We check B8G8R8A8_SRGB here because the swapchain format isn't selected yet.
     * After create_swapchain(), verify ctx.swapchain_format also supports blit
     * if it ends up being a different format (e.g., the SRGB fallback path). */
    CheckBlitSupport(Context.VulkanPhysicalDevice, VK_FORMAT_B8G8R8A8_SRGB);
}

bool HardwareRenderer::CreateLogicalDevice(VulkanContext &Context)
{
    std::set<uint32_t> UniqueFamilies = {Context.GraphicsFamily, Context.PresentFamily, Context.ComputeFamily};
    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
    float QueuePriority = 1.f;

    // Loop through all the unique queue families and fill in all queue creation info
    // These are similar to D3D11_XXX_DESC
    for (uint32_t Family : UniqueFamilies)
    {
        VkDeviceQueueCreateInfo QueueCreateInfo = {};
        QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCreateInfo.queueFamilyIndex = Family;
        QueueCreateInfo.queueCount = 1;
        QueueCreateInfo.pQueuePriorities = &QueuePriority;
        QueueCreateInfos.push_back(QueueCreateInfo);
    }

    /* Vulkan 1.4 features
    /* If your GPU doesn't support 1.4, remove this struct and change
    /* apiVersion in Module 1 to VK_API_VERSION_1_3.
    /* Note: Vulkan 1.4 headers are very new. If these field names don't compile,
    /* check your vulkan_core.h for the actual VkPhysicalDeviceVulkan14Features members.
    /* With Vulkan SDK 1.4.304+, these names should be correct. */
    VkPhysicalDeviceVulkan14Features Features_1_4{};
    Features_1_4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    Features_1_4.pushDescriptor = VK_TRUE;
    Features_1_4.maintenance5 = VK_TRUE;
    Features_1_4.maintenance6 = VK_TRUE;
    Features_1_4.indexTypeUint8 = VK_TRUE;

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features Features_1_3{};
    Features_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    Features_1_3.pNext = &Features_1_4;
    Features_1_3.dynamicRendering = VK_TRUE;
    Features_1_3.synchronization2 = VK_TRUE;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features Features_1_2{};
    Features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    Features_1_2.pNext = &Features_1_3;
    Features_1_2.bufferDeviceAddress = VK_TRUE;
    Features_1_2.scalarBlockLayout = VK_TRUE; // C-style shader structs

    // Base vulkan 1.0 features
    // Since we are using versioned feature structs(see above), we must set pEnabledFeataures to nullptr
    VkPhysicalDeviceFeatures2 Features_1_0{};
    Features_1_0.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    Features_1_0.pNext = &Features_1_2;

    // Fill the logical device create info
    VkDeviceCreateInfo CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
    CreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
    CreateInfo.pEnabledFeatures = nullptr;
    CreateInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    CreateInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    CreateInfo.pNext = &Features_1_0;

    if constexpr(ENABLE_VALIDATION_LAYERS)
    {
        CreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        CreateInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }

    VkResult Result = vkCreateDevice(Context.VulkanPhysicalDevice, &CreateInfo, nullptr, &Context.VulkanDevice);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to create logical device: " << Result << '\n';
        if (Result == VK_ERROR_FEATURE_NOT_PRESENT)
        {
            std::cerr << "Error: one of the vulkan features is not supported by your GPU or driver!" << '\n';
        }
        return false;
    }

    vkGetDeviceQueue(Context.VulkanDevice, Context.GraphicsFamily, 0, &Context.GraphicsQueue);
    vkGetDeviceQueue(Context.VulkanDevice, Context.PresentFamily, 0, &Context.PresentQueue);
    vkGetDeviceQueue(Context.VulkanDevice, Context.ComputeFamily, 0, &Context.ComputeQueue);

    return true;
}


bool HardwareRenderer::CreateSwapChain(VulkanContext &Context)
{
    VkSurfaceCapabilitiesKHR Capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context.VulkanPhysicalDevice, Context.VulkanSurface, &Capabilities);

    uint32_t FormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Context.VulkanPhysicalDevice, Context.VulkanSurface, &FormatCount, nullptr);
    // I just realized that we know the size
    // Why bother with a vector then
    std::vector<VkSurfaceFormatKHR> Formats(FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(Context.VulkanPhysicalDevice, Context.VulkanSurface, &FormatCount, Formats.data());

    uint32_t PresentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Context.VulkanPhysicalDevice, Context.VulkanSurface, &PresentModeCount, nullptr);
    std::vector<VkPresentModeKHR> PresentModes(PresentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(Context.VulkanPhysicalDevice, Context.VulkanSurface, &PresentModeCount, PresentModes.data());

    //Chooses format, we prefer B8G8RA SRGB
    VkSurfaceFormatKHR SurfaceFormat = Formats[0];
    for (const VkSurfaceFormatKHR& Format : Formats)
    {
        if (Format.format == VK_FORMAT_B8G8R8A8_SRGB && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            SurfaceFormat = Format;
            break;
        }
    }

    //Choose present mode, we prefer mailbox, fallback to FIFO
    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const VkPresentModeKHR& Mode : PresentModes)
    {
        if (Mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            PresentMode = Mode;
            break;
        }
    }

    //Choose extent
    VkExtent2D Extent = Capabilities.currentExtent;
    if (Extent.width == std::numeric_limits<uint32_t>::max())
    {
        int Width, Height;
        SDL_GetWindowSize(Context.Window, &Width, &Height);
        Extent.width = static_cast<uint32_t>(Width);
        Extent.height = static_cast<uint32_t>(Height);
        //Clamp the width and height
        Extent.width = std::clamp(Extent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        Extent.height = std::clamp(Extent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);

    }
    return true;

}

bool HardwareRenderer::CreateOffscreenTarget(VulkanContext &Context) {
}

bool HardwareRenderer::CreateOffScreenRenderPass(VulkanContext &Context) {
}

bool HardwareRenderer::CreateSwapChainRenderPass(VulkanContext &Context) {
}

bool HardwareRenderer::CreateFrameBuffers(VulkanContext &Context) {
}

bool HardwareRenderer::CreateCommandPool(VulkanContext &Context) {
}

bool HardwareRenderer::AllocateCommandBuffers(VulkanContext &Context) {
}

bool HardwareRenderer::CreateSyncObjects(VulkanContext &Context) {
}

bool HardwareRenderer::CleanUpContext(VulkanContext &Context) {
}


QueueFamilyIndices HardwareRenderer::FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR Surface)
{
    QueueFamilyIndices Indices;

    // Query available queue families
    uint32_t FamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &FamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> Families(FamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &FamilyCount, Families.data());

    for (uint32_t i = 0; i < FamilyCount; i++)
    {
        const VkQueueFamilyProperties& Family = Families[i];
        // Check for graphics queue support
        if (Family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            Indices.GraphicsFamily = i;
        }

        // Check for compute support
        // Prefer a dedicated compute queue (separate from graphics)
        // for better parallelism. Fall back to the graphics queue.
        if (Family.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (Indices.ComputeFamily == UINT32_MAX || !(Family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                // Prefer a compute-only family for true async compute.
                // If the current best also does graphics, replace it
                // with this one if this one doesn't do graphics.
                Indices.ComputeFamily = i;
            }
        }

        // Check for present support
        // This is a surface-specific query — different surfaces might
        // support different queue families for presentation.
        VkBool32 PresentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, Surface, &PresentSupport);
        if (PresentSupport)
        {
            Indices.PresentFamily = i;
        }
        // If we found everything, stop searching
        if (Indices.IsComplete())
        {
            break;
        }
    }

    return Indices;
}

// Helper: Check if a device supports all required extensions
bool HardwareRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice Device)
{
    uint32_t ExtCount = 0;
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtCount, nullptr);
    std::vector<VkExtensionProperties> Available(ExtCount);
    vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtCount, Available.data());

    for (const char* Required : DEVICE_EXTENSIONS)
    {
        bool IsFound = false;
        for (const VkExtensionProperties& Ext : Available)
        {
            if (std::strcmp(Required, Ext.extensionName) == 0)
            {
                IsFound = true;
                break;
            }
        }
        if (!IsFound)
        {
            return false;
        }
    }
    return true;
}

/* Helper: Check if all requested validation layers are available
 * The Vulkan SDK installs validation layers on your system, but they might
 * not be present on a user's machine (they don't ship with GPU drivers).
 * This function checks before we try to enable them. */
bool HardwareRenderer::CheckValidationLayers()
{
    //Query the number of available layers
    uint32_t LayerCount = 0;
    vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

    //Fill the layer array
    std::vector<VkLayerProperties> AvailableLayers(LayerCount);
    vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

    //Check if every layer we need is in the available list
    for (const char* LayerName : VALIDATION_LAYERS)
    {
        bool IsFound = false;
        for (const VkLayerProperties& layer : AvailableLayers)
        {
            if (std::strcmp(LayerName, layer.layerName) == 0)
            {
                IsFound = true;
                break;
            }
        }
        if (!IsFound)
        {
            std::cerr << "Validation layer not found: " << LayerName << '\n';
            return false;
        }

    }
    return true;
}

/* Helper: Debug callback function
 * This function is called by the validation layer whenever it detects an
 * issue. The parameters tell you what happened and how severe it is.
 * messageSeverity levels (from least to most severe):
 *   VERBOSE — Diagnostic info (very noisy, usually ignored)
 *   INFO    — Informational messages (resource creation, etc.)
 *  WARNING — Something that might be a bug but isn't necessarily wrong
 *   ERROR   — A definite Vulkan specification violation
 * messageType:
 *   GENERAL     — Unrelated to spec or performance
 *   VALIDATION  — Violates the Vulkan specification
 *  PERFORMANCE — Non-optimal use of Vulkan (e.g., unnecessary barriers)
 * We return VK_FALSE because returning VK_TRUE would abort the Vulkan call
 * that triggered the message, which is almost never what you want. */

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type, const VkDebugUtilsMessengerCallbackDataEXT* CallBackData,
	void* UserData)
{
	(void)Type;
	(void)UserData;
    // Color-code output by severity for easy scanning in the terminal
    if (Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::fprintf(stderr, "[VULKAN ERROR] %s\n", CallBackData->pMessage);
    }
    else if (Severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::fprintf(stderr, "[VULKAN WARNING] %s\n", CallBackData->pMessage);
    }
    else
    {
        // Uncomment the next line if need verbose/info messages.
        // They're very noisy but useful for debugging initialization issues.
        // std::printf("[VULKAN INFO] %s\n", pCallbackData->pMessage);
    }

    return VK_FALSE;
}


