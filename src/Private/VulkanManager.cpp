
#include <algorithm>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstring>  // for strcmp
#include <limits>
#include <iostream>
#include <format>
#include <set>
#include <fstream>

// Own headers
#include "VulkanManager.hpp"

#include "imgui_impl_vulkan.h"

VulkanManager::VulkanManager(){
}

VulkanManager::~VulkanManager() {
}

bool VulkanManager::Initialize()
{
    bool Result = false;
    Result = CreateInstance();
    if (!Result)
    {
        std::cerr << "CreateInstance failed: " << '\n';
        return Result;
    }

    return Result;
}



bool VulkanManager::CreateInstance()
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
    const VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &m_VulkanContext.VulkanInstance);

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

bool VulkanManager::SetupDebugMessenger()
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
    auto Func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_VulkanContext.VulkanInstance,
        "vkCreateDebugUtilsMessengerEXT"));
    if (!Func)
    {
        std::cerr << "Could not load vkCreateDebugUtilsMessengerEXT" << '\n';
        return false;
    }

    VkResult Result = Func(m_VulkanContext.VulkanInstance, &CreateInfo, nullptr, &m_VulkanContext.VulkanDebugMessenger);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to set up debug messenger: " << Result << '\n';
        return false;
    }

    std::cout << "Vulkan debug messenger created." << '\n';
    return true;
}

bool VulkanManager::CreateSurface()
{
    /* SDL3 creates the platform-specific Vulkan surface for us.
     * On Windows, this creates a VkWin32SurfaceKHR internally.
     * On Linux, it creates a VkXcbSurfaceKHR or VkWaylandSurfaceKHR.
     * The third parameter is the Vulkan memory allocator (nullptr = default). */

    // Note on surfaces - vulkan core is not platform specific. So it does not know how to interact with windows/GUI
    // To actually display things, we add a "surface" using the Windows System Integration extension
    // They represent an abstract surface that vulkan can use to display things to the user
    if (!SDL_Vulkan_CreateSurface(m_VulkanContext.Window, m_VulkanContext.VulkanInstance, nullptr, &m_VulkanContext.VulkanSurface))
    {
        std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << '\n';
        return false;
    }
    std::cout << "Vulkan surface created from SDL3 window." << '\n';
    return true;
}

bool VulkanManager::CheckBlitSupport(VkPhysicalDevice Device, VkFormat Format)
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

uint32_t VulkanManager::RatePhysicalDevices(VkPhysicalDevice Device, VkSurfaceKHR&Surface)
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
/*
 * A phsysical device refers to our GPU
 * We just pick the best one that supports vulkan 1.4
 */
bool VulkanManager::PickPhysicalDevice()
{
    uint32_t DeviceCount = 0;
    vkEnumeratePhysicalDevices(m_VulkanContext.VulkanInstance, &DeviceCount, nullptr);
    if (DeviceCount == 0)
    {
        std::cerr << "No Vulkan devices found!" << '\n';
        return false;
    }

    // Enumerate through all of the devices and pick the best
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    vkEnumeratePhysicalDevices(m_VulkanContext.VulkanInstance, &DeviceCount, Devices.data());

    uint32_t BestScore = 0;
    VkPhysicalDevice BestDevice = nullptr;

    for (const VkPhysicalDevice& Device : Devices)
    {
        VkPhysicalDeviceProperties Props;
        vkGetPhysicalDeviceProperties(Device, &Props);
        uint32_t Score = RatePhysicalDevices(Device, m_VulkanContext.VulkanSurface);
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

    m_VulkanContext.VulkanPhysicalDevice = BestDevice;
    QueueFamilyIndices Indices = FindQueueFamilies(BestDevice, m_VulkanContext.VulkanSurface);
    m_VulkanContext.GraphicsFamily = Indices.GraphicsFamily;
    m_VulkanContext.PresentFamily = Indices.PresentFamily;
    m_VulkanContext.ComputeFamily = Indices.ComputeFamily;

    /* Check blit support for B8G8R8A8_SRGB.
     * Note: We check B8G8R8A8_SRGB here because the swapchain format isn't selected yet.
     * After create_swapchain(), verify ctx.swapchain_format also supports blit
     * if it ends up being a different format (e.g., the SRGB fallback path). */
    CheckBlitSupport(m_VulkanContext.VulkanPhysicalDevice, VK_FORMAT_B8G8R8A8_SRGB);

    return true;
}

bool VulkanManager::CreateLogicalDevice()
{
    std::set<uint32_t> UniqueFamilies = {m_VulkanContext.GraphicsFamily, m_VulkanContext.PresentFamily, m_VulkanContext.ComputeFamily};
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
     * If your GPU doesn't support 1.4, remove this struct and change
     * apiVersion in Module 1 to VK_API_VERSION_1_3.
     * Note: Vulkan 1.4 headers are very new. If these field names don't compile,
     * check your vulkan_core.h for the actual VkPhysicalDeviceVulkan14Features members.
     * With Vulkan SDK 1.4.304+, these names should be correct. */
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

    VkResult Result = vkCreateDevice(m_VulkanContext.VulkanPhysicalDevice, &CreateInfo, nullptr,
        &m_VulkanContext.VulkanDevice);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to create logical device: " << Result << '\n';
        if (Result == VK_ERROR_FEATURE_NOT_PRESENT)
        {
            std::cerr << "Error: one of the vulkan features is not supported by your GPU or driver!" << '\n';
        }
        return false;
    }

    vkGetDeviceQueue(m_VulkanContext.VulkanDevice, m_VulkanContext.GraphicsFamily, 0, &m_VulkanContext.GraphicsQueue);
    vkGetDeviceQueue(m_VulkanContext.VulkanDevice, m_VulkanContext.PresentFamily, 0, &m_VulkanContext.PresentQueue);
    vkGetDeviceQueue(m_VulkanContext.VulkanDevice, m_VulkanContext.ComputeFamily, 0, &m_VulkanContext.ComputeQueue);

    return true;
}


bool VulkanManager::CreateSwapChain()
{
    VkSurfaceCapabilitiesKHR Capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_VulkanContext.VulkanPhysicalDevice, m_VulkanContext.VulkanSurface, &Capabilities);

    uint32_t FormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanContext.VulkanPhysicalDevice, m_VulkanContext.VulkanSurface, &FormatCount, nullptr);
    // I just realized that we know the size
    // Why bother with a vector then
    std::vector<VkSurfaceFormatKHR> Formats(FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_VulkanContext.VulkanPhysicalDevice, m_VulkanContext.VulkanSurface, &FormatCount, Formats.data());

    uint32_t PresentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanContext.VulkanPhysicalDevice, m_VulkanContext.VulkanSurface, &PresentModeCount, nullptr);
    std::vector<VkPresentModeKHR> PresentModes(PresentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_VulkanContext.VulkanPhysicalDevice, m_VulkanContext.VulkanSurface, &PresentModeCount, PresentModes.data());

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

    //Choose extent - the dimensions of the swapchain image
    VkExtent2D Extent = Capabilities.currentExtent;
    if (Extent.width == std::numeric_limits<uint32_t>::max())
    {
        int Width, Height;
        SDL_GetWindowSize(m_VulkanContext.Window, &Width, &Height);
        Extent.width = static_cast<uint32_t>(Width);
        Extent.height = static_cast<uint32_t>(Height);
        //Clamp the width and height
        Extent.width = std::clamp(Extent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        Extent.height = std::clamp(Extent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);
    }

    uint32_t ImageCount = Capabilities.minImageCount + 1;
    if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount)
    {
        // Clamp the the ImageCount so it doesn't go over our max supported swapchain buffer counts
        ImageCount = Capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR CreateInfo = {};
    CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    CreateInfo.surface = m_VulkanContext.VulkanSurface;
    CreateInfo.minImageCount = ImageCount;
    CreateInfo.imageFormat = SurfaceFormat.format;
    CreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
    CreateInfo.imageExtent = Extent;
    CreateInfo.imageArrayLayers = 1;

    // We need to BLIT to the swapchain
    // (offscreen → swapchain), so the swapchain must accept transfers.
    // COLOR_ATTACHMENT_BIT allows us to draw directly onto the swap chain image(fancy way of saying RenderTarget)
    // TRANSFER_DST_BIT indicates that the the write to the image would be done using transfer operation(blit and copy)
    CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    uint32_t FamilyIndices[] = {m_VulkanContext.GraphicsFamily, m_VulkanContext.PresentFamily}; // This is about swapchain so compute family isn't involved
    if (m_VulkanContext.GraphicsFamily != m_VulkanContext.PresentFamily)
    {
        // When the graphics and present queue are not the same, we need to coordinate between them
        // So multiple queue families can access the image
        CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        CreateInfo.queueFamilyIndexCount = 2;
        CreateInfo.pQueueFamilyIndices = FamilyIndices;
    }
    else
    {
        // Graphics and present queue are the same, so no need for concurrent access
        CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    CreateInfo.preTransform = Capabilities.currentTransform;
    CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    CreateInfo.presentMode = PresentMode;
    CreateInfo.clipped = VK_TRUE; // Note on VK_TRUE, it's 1U, so I am not replacing it using just bool
    // Pass the old swapchain so the driver can reuse resources.
    // On first creation, ctx.swapchain is nullptr (initialized in VulkanContext).
    // On recreation, it holds the old handle — the driver recycles internal structures.
    CreateInfo.oldSwapchain = m_VulkanContext.SwapChain;

    if (vkCreateSwapchainKHR(m_VulkanContext.VulkanDevice, &CreateInfo, nullptr,
        &m_VulkanContext.SwapChain) != VK_SUCCESS)
    {
        std::cerr << "Failed to create swapchain!" << '\n';
        return false;
    }
    // Set the states in the context struct
    m_VulkanContext.SwapChainFormat = SurfaceFormat.format;
    m_VulkanContext.SwapChainExtent = Extent;

    vkGetSwapchainImagesKHR(m_VulkanContext.VulkanDevice, m_VulkanContext.SwapChain, &ImageCount, nullptr);
    m_VulkanContext.SwapChainImages.reserve(ImageCount);
    vkGetSwapchainImagesKHR(m_VulkanContext.VulkanDevice, m_VulkanContext.SwapChain, &ImageCount, m_VulkanContext.SwapChainImages.data());

    m_VulkanContext.SwapChainImageViews.reserve(ImageCount);

    for (size_t i = 0; i < ImageCount; i++)
    {
        VkImageViewCreateInfo ViewCreateInfo = {};
        ViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ViewCreateInfo.image = m_VulkanContext.SwapChainImages[i];
        ViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ViewCreateInfo.format = m_VulkanContext.SwapChainFormat;
        ViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ViewCreateInfo.subresourceRange.baseMipLevel = 0;
        ViewCreateInfo.subresourceRange.levelCount = 1;
        ViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        ViewCreateInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_VulkanContext.VulkanDevice, &ViewCreateInfo, nullptr,
            &m_VulkanContext.SwapChainImageViews[i]) != VK_SUCCESS)
        {
            std::cerr << "Failed to create swapchain image view for image " << i << '\n';
            return false;
        }
    }


    return true;

}

bool VulkanManager::CreateOffscreenTarget(uint32_t Width, uint32_t Height) {
    m_VulkanContext.OffScreen.Width = Width;
    m_VulkanContext.OffScreen.Height = Height;

    // Create the image
    // COLOR_ATTACHMENT: we render to it
    // TRANSFER_SRC: we blit from it to the swapchain
    VkImageCreateInfo ImageInfo = {};
    ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = m_VulkanContext.OffScreen.Format;
    ImageInfo.extent.width = Width;
    ImageInfo.extent.height = Height;
    ImageInfo.extent.depth = 1;
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_VulkanContext.VulkanDevice, &ImageInfo, nullptr,
        &m_VulkanContext.OffScreen.VkImage) != VK_SUCCESS)
    {
        std::cerr << "Failed to create offscreen image!" << '\n';
        return false;
    }

    // Allocate device-local memory (device local - fast VRAM which can't be mapped to CPU)
    VkMemoryRequirements MemReqs;
    vkGetImageMemoryRequirements(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.VkImage, &MemReqs);

    VkMemoryAllocateInfo AllocInfo = {};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemReqs.size;
    AllocInfo.memoryTypeIndex = FindMemoryType(m_VulkanContext.VulkanPhysicalDevice, MemReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (AllocInfo.memoryTypeIndex == UINT32_MAX)
    {
        std::cerr << "Failed to find a suitable memory type for offscreen image!" << '\n';
        return false;
    }
    if (vkAllocateMemory(m_VulkanContext.VulkanDevice, &AllocInfo, nullptr,
        &m_VulkanContext.OffScreen.VkMemory) != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate offscreen memory!" << '\n';
        return false;
    }

    // Create the image view, views are objects that are used to interface with GPU memory
    // They tell vulkan what's the format, how are we viewing it, and what part of the memory we want to access
    VkImageViewCreateInfo ViewInfo = {};
    ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ViewInfo.image = m_VulkanContext.OffScreen.VkImage;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = m_VulkanContext.OffScreen.Format;
    ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ViewInfo.subresourceRange.baseMipLevel = 0;
    ViewInfo.subresourceRange.levelCount = 1;
    ViewInfo.subresourceRange.baseArrayLayer = 0;
    ViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_VulkanContext.VulkanDevice, &ViewInfo, nullptr,
        &m_VulkanContext.OffScreen.VkImageView) != VK_SUCCESS)
    {
        std::cerr << "Failed to create offscreen image view!" << '\n';
        return false;
    }

    // Create the frame buffer and bind it to the first pass(particles)
    VkFramebufferCreateInfo FrameBufferCreateInfo = {};
    FrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FrameBufferCreateInfo.renderPass = m_VulkanContext.OffScreen.RenderPass;
    FrameBufferCreateInfo.attachmentCount = 1;
    FrameBufferCreateInfo.pAttachments = &m_VulkanContext.OffScreen.VkImageView;
    FrameBufferCreateInfo.width = Width;
    FrameBufferCreateInfo.height = Height;
    FrameBufferCreateInfo.layers = 1;
    if (vkCreateFramebuffer(m_VulkanContext.VulkanDevice, &FrameBufferCreateInfo, nullptr,
        &m_VulkanContext.OffScreen.Framebuffer) != VK_SUCCESS)
    {
        std::cerr << "Failed to create offscreen framebuffer!" << '\n';
        return false;
    }

    return true;
}

bool VulkanManager::CreateOffScreenRenderPass()
{
    // Man I miss when I can just OMSetRenderTarget() + Draw() in DX11
    // Like what do you mean I have to set up the target AND the entire pass object


    // Offscreen supersampling target where the particles will go directly
    // Its format matches the swap chain for blit compatibility
    VkAttachmentDescription OffscreenAttachmentDesc = {};
    OffscreenAttachmentDesc.format = m_VulkanContext.SwapChainFormat;
    OffscreenAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    OffscreenAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // We clear the entire target image each frame
    OffscreenAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    OffscreenAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't use the stencil buffer
    OffscreenAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // No stencil
    OffscreenAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // This is a key flag, it tells vulkan that we will be transfering FROM this image
    OffscreenAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    // An attachment reference tells vulkan which subpass uses which slot in the description array
    VkAttachmentReference OffscreenAttachmentRef = {};
    OffscreenAttachmentRef.attachment = 0;
    OffscreenAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription OffscreenSubpassDesc = {};
    OffscreenSubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    OffscreenSubpassDesc.colorAttachmentCount = 1;
    OffscreenSubpassDesc.pColorAttachments = &OffscreenAttachmentRef;

    // Two dependencies: incoming (external → subpass 0) and outgoing (subpass 0 → external)
    VkSubpassDependency OffscreenSubpassDep[2]{}; //Note: DO NOT leave vulkan stuffs uninitialized, USE BRACES!

    // All the following dependencies voodoos are just synchronization rules that let vulkan knows:
    // "Hey I want a to be done before b gets started"


    // External → Subpass 0
    // Wait for any prior color attachment work to finish before we start writing
    OffscreenSubpassDep[0].srcSubpass = VK_SUBPASS_EXTERNAL; // This means "ANYTHING outside of this pass"
    OffscreenSubpassDep[0].dstSubpass = 0; // First subpass
    OffscreenSubpassDep[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Which stage to wait/block
    OffscreenSubpassDep[0].srcAccessMask = 0; // Which memory operation r/w to synchronize
    OffscreenSubpassDep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    OffscreenSubpassDep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Subpass 0 → External
    // Synchronize color attachment writes with the subsequent blit read.
    // Without this, the blit may read stale data from the offscreen image
    OffscreenSubpassDep[1].srcSubpass = 0;
    OffscreenSubpassDep[1].dstSubpass = VK_SUBPASS_EXTERNAL; // Whatever comes AFTER our drawing pass
    OffscreenSubpassDep[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    OffscreenSubpassDep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    OffscreenSubpassDep[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    OffscreenSubpassDep[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    // Fill the create info and create as usual
    VkRenderPassCreateInfo OffscreenRenderPassCreateInfo = {};
    OffscreenRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    OffscreenRenderPassCreateInfo.attachmentCount = 1;
    OffscreenRenderPassCreateInfo.pAttachments = &OffscreenAttachmentDesc;
    OffscreenRenderPassCreateInfo.subpassCount = 1;
    OffscreenRenderPassCreateInfo.pSubpasses = &OffscreenSubpassDesc;
    OffscreenRenderPassCreateInfo.dependencyCount = 2;
    OffscreenRenderPassCreateInfo.pDependencies = OffscreenSubpassDep;

    if (vkCreateRenderPass(m_VulkanContext.VulkanDevice, &OffscreenRenderPassCreateInfo, nullptr, &m_VulkanContext.OffScreen.RenderPass) != VK_SUCCESS)
    {
        std::cerr << "Failed to create offscreen render pass!" << '\n';
        return false;
    }
    return true;
}

bool VulkanManager::CreateSwapChainRenderPass()
{
    VkAttachmentDescription ColorAttachmentDesc = {};
    ColorAttachmentDesc.format = m_VulkanContext.SwapChainFormat;
    ColorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

    // The swap chain already has the blitted image containing the particles
    // So we load instead of clear here
    ColorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ColorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ColorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // This time we presents

    VkAttachmentReference ColorAttachmentRef = {};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription SwapchainSubpassDesc = {};
    SwapchainSubpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SwapchainSubpassDesc.colorAttachmentCount = 1;
    SwapchainSubpassDesc.pColorAttachments = &ColorAttachmentRef;

    // No external dependency needed — the barrier before this pass
    // (in the command buffer) already handles synchronization.
    VkSubpassDependency SwapchainSubpassDep = {};
    SwapchainSubpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
    SwapchainSubpassDep.dstSubpass = 0;
    SwapchainSubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SwapchainSubpassDep.srcAccessMask = 0;
    SwapchainSubpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    SwapchainSubpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo SwapchainRenderPassCreateInfo = {};
    SwapchainRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    SwapchainRenderPassCreateInfo.attachmentCount = 1;
    SwapchainRenderPassCreateInfo.pAttachments = &ColorAttachmentDesc;
    SwapchainRenderPassCreateInfo.subpassCount = 1;
    SwapchainRenderPassCreateInfo.pSubpasses = &SwapchainSubpassDesc;
    SwapchainRenderPassCreateInfo.dependencyCount = 1;
    SwapchainRenderPassCreateInfo.pDependencies = &SwapchainSubpassDep;

    if (vkCreateRenderPass(m_VulkanContext.VulkanDevice, &SwapchainRenderPassCreateInfo, nullptr, &m_VulkanContext.SwapChainRenderPass) != VK_SUCCESS)
    {
        std::cerr << "Failed to create swapchain render pass!" << '\n';
        return false;
    }
    return true;
}

bool VulkanManager::CreateFrameBuffers()
{
    m_VulkanContext.FrameBuffers.resize(m_VulkanContext.SwapChainImages.size());
    for (size_t i = 0; i < m_VulkanContext.SwapChainImages.size(); i++)
    {
        VkImageView Attachments[] = { m_VulkanContext.SwapChainImageViews[i] };

        VkFramebufferCreateInfo FrameBufferCreateInfo = {};
        FrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FrameBufferCreateInfo.renderPass = m_VulkanContext.SwapChainRenderPass;
        FrameBufferCreateInfo.attachmentCount = 1;
        FrameBufferCreateInfo.pAttachments = Attachments;
        FrameBufferCreateInfo.width = m_VulkanContext.SwapChainExtent.width;
        FrameBufferCreateInfo.height = m_VulkanContext.SwapChainExtent.height;
        FrameBufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(m_VulkanContext.VulkanDevice, &FrameBufferCreateInfo, nullptr,
            &m_VulkanContext.FrameBuffers[i]) != VK_SUCCESS)
        {
            std::cerr << "Failed to create framebuffer for swapchain image " << i << '\n';
            return false;
        }
    }

    return true;
}

bool VulkanManager::CreateGraphicsPipeline()
{
    std::vector<char> VertexShaderBuffer;
    std::vector<char> FragmentShaderBuffer;
    bool Result = ReadCompiledShader("Shaders/Particles.vert.spv", VertexShaderBuffer);
    if (!Result)
    {
        std::cerr << "Failed to create graphics pipeline!" << '\n';
        return false;
    }
    Result = ReadCompiledShader("Shaders/Particles.frag.spv", FragmentShaderBuffer);
    if (!Result)
    {
        return false;
    }

    VkShaderModule VertexShaderModule;
    VkShaderModule FragmentShaderModule;
    if (!CreateShaderModule(VertexShaderBuffer, VertexShaderModule))
    {
        std::cerr << "Failed to create graphics pipeline!" << '\n';
        return false;
    }
    if (!CreateShaderModule(FragmentShaderBuffer, FragmentShaderModule))
    {
        // Free up the already created module
        vkDestroyShaderModule(m_VulkanContext.VulkanDevice, VertexShaderModule, nullptr);
        std::cerr << "Failed to create graphics pipeline!" << '\n';
        return false;
    }

    // Set up shader stages for both vertex and fragment
    VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfos[2] = {};
    PipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    PipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT; // Vertex shader
    PipelineShaderStageCreateInfos[0].module = VertexShaderModule;
    PipelineShaderStageCreateInfos[0].pName = "main"; // Entry point

    PipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    PipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; // Fragment shader
    PipelineShaderStageCreateInfos[1].module = FragmentShaderModule;
    PipelineShaderStageCreateInfos[1].pName = "main";

    // Set up vertex input, similar to DX11's setting up input buffer for vertex shader
    VkVertexInputBindingDescription VertexBindingDescriptions[2] = {};
    VertexBindingDescriptions[0].binding = 0; // Slot 0, this refers to the POSITION0 in HLSL
    VertexBindingDescriptions[0].stride = sizeof(float) * 2; // Size of the data in POSITION0, which is a float2
    VertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    // Instance data, binding starts at slot 1, contains instance position, color, and size = 3 + 4 + 1 = 8 floats
    VertexBindingDescriptions[1].binding = 1;
    VertexBindingDescriptions[1].stride = sizeof(float) * 8;
    VertexBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription AttributeDescriptions[4] = {};
    AttributeDescriptions[0].location = 0;
    AttributeDescriptions[0].binding = 0;
    AttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // Vertex position, again, just two floats
    AttributeDescriptions[0].offset = 0;
    // Instance data
    // Instance position
    AttributeDescriptions[1].location = 1;
    AttributeDescriptions[1].binding = 1;
    AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // Instance position, the center of the particle quad
    AttributeDescriptions[1].offset = 0; // We are the first at OUR BINDING slot, no offset needed
    // Instance color
    AttributeDescriptions[2].location = 2;
    AttributeDescriptions[2].binding = 1;
    AttributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; // Instance color, four floats
    AttributeDescriptions[2].offset = sizeof(float) * 3; // Jump over the instance position float3
    // Instance size
    AttributeDescriptions[3].location = 3;
    AttributeDescriptions[3].binding = 1;
    AttributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
    AttributeDescriptions[3].offset = sizeof(float) * 7; // Jump over a float3 and a float4

    VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {};
    VertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertexInputStateCreateInfo.vertexBindingDescriptionCount = 2; // We got two different binding descriptions, vertex and instance
    VertexInputStateCreateInfo.pVertexBindingDescriptions = VertexBindingDescriptions;
    VertexInputStateCreateInfo.vertexAttributeDescriptionCount = 4; // We have 4 different attribute descriptions, 1 vertex + 3 instance varibles
    VertexInputStateCreateInfo.pVertexAttributeDescriptions = AttributeDescriptions;

    // Create input assembler, viewport state, rasterizer and multisampling
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {};
    InputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // We can also use a triangle strip in our usage
    InputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
    ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewportStateCreateInfo.viewportCount = 1;
    ViewportStateCreateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {};
    RasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    RasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    RasterizationStateCreateInfo.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {};
    MultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // We use additive blending for the particles
    // Meaning that when two particles overlap on a single pixel/fragment, we add their color
    // Instead of doing depth testing and occlusion
    VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {};
    ColorBlendAttachmentState.blendEnable = VK_TRUE;
    ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};
    ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendStateCreateInfo.attachmentCount = 1;
    ColorBlendStateCreateInfo.pAttachments = &ColorBlendAttachmentState;

    // Dynamic pipeline states allow us
    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {};
    DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    DynamicStateCreateInfo.dynamicStateCount = 2;
    DynamicStateCreateInfo.pDynamicStates = DynamicStates;

    // Set up push constants - these are similar to DX11 constant buffers
    // Key difference is speed, they get pushed to GPU registers instead of getting copied into VRAM
    // The get embedded inside the command buffer and got pushed straight onto GPU registers
    VkPushConstantRange PushConstantRange = {};
    PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    PushConstantRange.offset = 0;
    PushConstantRange.size = 96; // Push constants max size is 128 to 256 bytes

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};
    PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    PipelineLayoutCreateInfo.pPushConstantRanges = &PushConstantRange;

    if (vkCreatePipelineLayout(m_VulkanContext.VulkanDevice, &PipelineLayoutCreateInfo, nullptr,
        &m_VulkanContext.GraphicsPipelineLayout) != VK_SUCCESS)
    {
        // Need to release the shader modules first before we quit in case of errors
        vkDestroyShaderModule(m_VulkanContext.VulkanDevice, VertexShaderModule, nullptr);
        vkDestroyShaderModule(m_VulkanContext.VulkanDevice, FragmentShaderModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};
    PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineCreateInfo.stageCount = 2;
    PipelineCreateInfo.pStages = PipelineShaderStageCreateInfos;
    PipelineCreateInfo.layout = m_VulkanContext.GraphicsPipelineLayout;
    PipelineCreateInfo.renderPass = m_VulkanContext.OffScreen.RenderPass; // We are rendering to an offscreen target, not directly to the swapchain
    PipelineCreateInfo.subpass = 0;
    PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCreateInfo.basePipelineIndex = -1;
    PipelineCreateInfo.pVertexInputState = &VertexInputStateCreateInfo;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyStateCreateInfo;
    PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
    PipelineCreateInfo.pRasterizationState = &RasterizationStateCreateInfo;
    PipelineCreateInfo.pMultisampleState = &MultisampleStateCreateInfo;
    PipelineCreateInfo.pDepthStencilState = nullptr; // No depth or stencil buffer
    PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
    PipelineCreateInfo.pDynamicState = &DynamicStateCreateInfo;

    VkResult vkResult = vkCreateGraphicsPipelines(m_VulkanContext.VulkanDevice, VK_NULL_HANDLE, 1,
        &PipelineCreateInfo, nullptr, &m_VulkanContext.GraphicsPipeline);
    if (vkResult != VK_SUCCESS)
    {
        std::cerr << "Failed to create graphics pipeline! VkResult: " << vkResult << '\n';
        return false;
    }

    return true;
}

bool VulkanManager::CreateCommandPool()
{
    // A
    VkCommandPoolCreateInfo CommandPoolCreateInfo = {};
    CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCreateInfo.queueFamilyIndex = m_VulkanContext.GraphicsFamily;
    CommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const VkResult Result = vkCreateCommandPool(m_VulkanContext.VulkanDevice, &CommandPoolCreateInfo,
        nullptr, &m_VulkanContext.CommandPool);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to create command pool! VkResult: " << Result << '\n';
        return false;
    }
    return true;
}

bool VulkanManager::AllocateCommandBuffers()
{
    /*
     * A command buffer is well... where all of our commands to the GPU gets stored
     * Unlike DX11 and OpenGL, where we issue commands sequentially through some kind of API construct
     * such as D3D11Device and D3D11DeviceContext
     * DX12 and Vulkan requires us to be explicit and submit every command ourself
     */
    m_VulkanContext.CommandBuffers.resize(VulkanManager::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {};
    CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool = m_VulkanContext.CommandPool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_VulkanContext.CommandBuffers.size());
    const VkResult Result = vkAllocateCommandBuffers(m_VulkanContext.VulkanDevice,
        &CommandBufferAllocateInfo, m_VulkanContext.CommandBuffers.data());
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate command buffers! VkResult: " << Result << '\n';
        return false;
    }
    return true;
}

bool VulkanManager::CreateParticleInstanceBuffers(uint32_t NumMaxParticles)
{
    /*
     * The storage buffer holds NumMaxParticles floats. We got 55000 x 4 = 220000 bytes in each one
     * We got 9 buffers and 2 frames in flight.
     */
    const VkDeviceSize BufferSize = NumMaxParticles * 4;

    /*
     * Properties for the GPU VRAM that will hold the buffers
     * HOST_VISIBLE, basially dx11's CPU_READ_WRITE
     * HOST_COHERENT, allows syncing between GPU and CPU without explicit
     * vkFlushMappedMemoryRanges calls. This is a bit slower
     */
    const VkMemoryPropertyFlags MemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    // Usage: STROAGE_BUFFER_BIT tells the shader to declare it ass StructuredBuffer
    // No need for Transfer_src/dest bit because we are not copying, we are using CPU
    // to write directly via mapped pointers
    const VkBufferUsageFlags UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    for (uint32_t i = 0; i < VulkanContext::MAX_FRAMES_IN_FLIGHT; i++)
    {
        ParticleInstanceBuffers& FrameInstanceBufferRef = m_VulkanContext.ParticleInstanceBufferArray[i];
        // Get the pointers out of the ref
        // Allocate all 9 buffers. The order must match
        AllocatedVkBuffer* Buffers[9] = {
            &FrameInstanceBufferRef.Px, &FrameInstanceBufferRef.Py, &FrameInstanceBufferRef.Pz,
            &FrameInstanceBufferRef.R, &FrameInstanceBufferRef.G, &FrameInstanceBufferRef.B,
            &FrameInstanceBufferRef.Size, &FrameInstanceBufferRef.LifeTime, &FrameInstanceBufferRef.MaxLifeTime
        };
        for (uint32_t j = 0; j < 9; j++)
        {
            *Buffers[j] = CreateBuffer(BufferSize, UsageFlags, MemoryProperties);
            if (!Buffers[j]->VulkanBuffer)
            {
                std::fprintf(stderr, "Failed to allocate particle SoA buffer %u for frame %u\n", j, i);
                return false;
            }
            /*
             * We do persistant mapping. Call vkMapMemory once at this init, keep our mapped pointer
             * So the buffer stays mapped until we free the memory. And we do not have to suffer from
             * per frame map and unmap overhead
             */
            VkResult MapResult = vkMapMemory(m_VulkanContext.VulkanDevice, Buffers[i]->VulkanDeviceMemory, 0, BufferSize,
                0, &FrameInstanceBufferRef.MappedPtr[j]);
            if (MapResult != VK_SUCCESS)
            {
                std::cerr << "Failed to map particle buffer! VkResult: " << MapResult << '\n';
            }
        }
    }
}


bool VulkanManager::CreateParticleDescriptorLayout()
{

}

AllocatedVkBuffer VulkanManager::CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                              VkMemoryPropertyFlags Properties)
{
    AllocatedVkBuffer Buffer;
    Buffer.VulkanBufferSize = Size;

    // Create the buffer object, same deal as before, fill in the create info
    VkBufferCreateInfo BufferCreateInfo = {};
    BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BufferCreateInfo.usage = Usage;
    BufferCreateInfo.size = Size;
    BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult Result = vkCreateBuffer(m_VulkanContext.VulkanDevice, &BufferCreateInfo,
        nullptr, &Buffer.VulkanBuffer);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to create buffer! VkResult: " << Result << '\n';
        return Buffer;
    }

    /*
     * Allocate the actual memory for the buffer
     * At this point the buffer has no existing place in the memory yet
     * So we need to check its memory requirements and allocate the memory
     * Like who designed this part, what "create buffer" function does not allocate the memory ITSELF???
     */
    VkMemoryRequirements BufferMemoryRequirements;
    vkGetBufferMemoryRequirements(m_VulkanContext.VulkanDevice, Buffer.VulkanBuffer, &BufferMemoryRequirements);

    // Fill in the create info and allocate the buffer
    VkMemoryAllocateInfo MemoryAllocateInfo = {};
    MemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemoryAllocateInfo.allocationSize = BufferMemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = FindMemoryType(m_VulkanContext.VulkanPhysicalDevice,
        BufferMemoryRequirements.memoryTypeBits, Properties);

    Result = vkAllocateMemory(m_VulkanContext.VulkanDevice, &MemoryAllocateInfo,
        nullptr, &Buffer.VulkanDeviceMemory);
    if (Result != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate memory! VkResult: " << Result << '\n';
        // Destroy the buffer
        vkDestroyBuffer(m_VulkanContext.VulkanDevice, Buffer.VulkanBuffer, nullptr);
        // Set the buffer ptr to nullptr
        Buffer.VulkanBuffer = nullptr;
        return Buffer;
    }

    // Bind the actual memory to the buffer
    vkBindBufferMemory(m_VulkanContext.VulkanDevice, Buffer.VulkanBuffer, Buffer.VulkanDeviceMemory, 0);

    return Buffer;
}

void VulkanManager::GPUCopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size)
{
    // Create a temporary command buffer for the transfer

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {};
    CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CommandBufferAllocateInfo.commandPool = m_VulkanContext.CommandPool;
    CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer CopyCommandBuffer;
    vkAllocateCommandBuffers(m_VulkanContext.VulkanDevice, &CommandBufferAllocateInfo, &CopyCommandBuffer);

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // This flag tells vulkan that we use the command buffer for a one time submission
    // So vulkan can do some optimizations since we won't reuse this(less states, less memory usage)
    CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(CopyCommandBuffer, &CommandBufferBeginInfo);

    VkBufferCopy CopyRegion = {};
    CopyRegion.size = Size;
    vkCmdCopyBuffer(CopyCommandBuffer, SrcBuffer, DstBuffer, 1, &CopyRegion);

    vkEndCommandBuffer(CopyCommandBuffer);

    // Submit the command buffer and wait
    // Wait because vkQueueWaitIdle will block CPU until the GPU is done
    // This is ok for one time memory transfer
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CopyCommandBuffer;

    vkQueueSubmit(m_VulkanContext.GraphicsQueue, 1, &SubmitInfo, nullptr);
    vkQueueWaitIdle(m_VulkanContext.GraphicsQueue);

    vkFreeCommandBuffers(m_VulkanContext.VulkanDevice, m_VulkanContext.CommandPool, 1, &CopyCommandBuffer);
}

// This function creates a device-local buffer with initial data in it
// We can use such buffers for user staging(fancy way to say preping some data for later use by the GPU I guess)
AllocatedVkBuffer VulkanManager::CreateBufferWithData(const void* Data, VkDeviceSize Size, VkBufferUsageFlags Usage)
{
    // Create a staging buffer with CPU write
    AllocatedVkBuffer StagingBuffer = CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (StagingBuffer.VulkanBuffer == nullptr)
    {
        return StagingBuffer;
    }

    // Similar to DX11, we have to first map a buffer before putting data into it
    void* MappedPtr;
    vkMapMemory(m_VulkanContext.VulkanDevice, StagingBuffer.VulkanDeviceMemory, 0, Size, 0, &MappedPtr);
    memcpy(MappedPtr, Data, Size);
    // Remember to unmap
    vkUnmapMemory(m_VulkanContext.VulkanDevice, StagingBuffer.VulkanDeviceMemory);

    // Create the device local buffer, this is the buffer that lives on the GPU
    // And it's the destination of all these transfer operations
    AllocatedVkBuffer DeviceLocalBuffer = CreateBuffer(Size, Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (DeviceLocalBuffer.VulkanBuffer == nullptr)
    {
        return DeviceLocalBuffer;
    }
    // Use GPU to copy from the staging buffer
    GPUCopyBuffer(StagingBuffer.VulkanBuffer, DeviceLocalBuffer.VulkanBuffer, Size);
    // Free the staging buffer so it's a one time resource for this copying operation
}

// Small helper to clean up the custom AllocatedVkBuffer struct
void VulkanManager::DestroyBuffer(AllocatedVkBuffer& VulkanBuffer)
{
    if (VulkanBuffer.VulkanBuffer)
    {
        vkDestroyBuffer(m_VulkanContext.VulkanDevice, VulkanBuffer.VulkanBuffer, nullptr);
    }
    if (VulkanBuffer.VulkanDeviceMemory)
    {
        vkFreeMemory(m_VulkanContext.VulkanDevice, VulkanBuffer.VulkanDeviceMemory, nullptr);
    }
}

bool VulkanManager::CreateSyncObjects() {
    // These are similar to mutexes we learned in class
    m_VulkanContext.ImageAvailableSemaphores.resize(VulkanManager::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_VulkanContext.RenderFinishedSemaphores.resize(VulkanManager::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_VulkanContext.InFlightFence.resize(VulkanManager::VulkanContext::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo FenceCreateInfo = {};
    FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult Result;
    for (uint32_t i = 0; i < VulkanManager::VulkanContext::MAX_FRAMES_IN_FLIGHT; i++)
    {
        Result = vkCreateSemaphore(m_VulkanContext.VulkanDevice, &SemaphoreCreateInfo,
            nullptr, &m_VulkanContext.ImageAvailableSemaphores[i]);
        if (Result != VK_SUCCESS)
        {
            std::cerr << "Failed to create image available semaphore! VkResult: " << Result << '\n';
            return false;
        }
        Result = vkCreateSemaphore(m_VulkanContext.VulkanDevice, &SemaphoreCreateInfo,
            nullptr, &m_VulkanContext.RenderFinishedSemaphores[i]);
        if (Result != VK_SUCCESS)
        {
            std::cerr << "Failed to create render finished semaphore! VkResult: " << Result << '\n';
            return false;
        }
        Result = vkCreateFence(m_VulkanContext.VulkanDevice, &FenceCreateInfo,
            nullptr, &m_VulkanContext.InFlightFence[i]);
        if (Result != VK_SUCCESS)
        {
            std::cerr << "Failed to create in flight fence! VkResult: " << Result << '\n';
            return false;
        }
    }
    return true;
}

bool VulkanManager::CleanUpContext()
{
    return false;
}

void VulkanManager::DestoryOffscreenTarget()
{
    // The clean up of the offscreen target HAS To be done in this order
    // Otherwise vulkan flips out and app goes boom

    if (m_VulkanContext.OffScreen.Framebuffer != nullptr)
    {
        vkDestroyFramebuffer(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.Framebuffer, nullptr);
    }
    if (m_VulkanContext.OffScreen.RenderPass != nullptr)
    {
        vkDestroyRenderPass(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.RenderPass, nullptr);
    }
    if (m_VulkanContext.OffScreen.VkImageView != nullptr)
    {
        vkDestroyImageView(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.VkImageView, nullptr);
    }
    if (m_VulkanContext.OffScreen.VkImage != nullptr)
    {
        vkDestroyImage(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.VkImage, nullptr);
    }
    if (m_VulkanContext.OffScreen.VkMemory != nullptr)
    {
        vkFreeMemory(m_VulkanContext.VulkanDevice, m_VulkanContext.OffScreen.VkMemory, nullptr);
    }
    m_VulkanContext.OffScreen = {};
}


QueueFamilyIndices VulkanManager::FindQueueFamilies(VkPhysicalDevice Device, VkSurfaceKHR Surface)
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
bool VulkanManager::CheckDeviceExtensionSupport(VkPhysicalDevice Device)
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
bool VulkanManager::CheckValidationLayers()
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

uint32_t VulkanManager::FindMemoryType(VkPhysicalDevice PhysicalDevice, uint32_t TypeFilter,
    VkMemoryPropertyFlags Properties)
{
    // This function gets athe memory types supported by our gpu
    // Then check whether each memory type compatible with our resources
    // And whether it has all the properties

    // And yes the TypeFilter is returned as bit mask, evil :(
    VkPhysicalDeviceMemoryProperties MemProps;
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemProps);
    for (uint32_t i = 0; i < MemProps.memoryTypeCount; i++)
    {
        // Look
        if ((TypeFilter & (1 << i)) && (MemProps.memoryTypes[i].propertyFlags & Properties) == Properties)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanManager::ReadCompiledShader(const std::string &ShaderBinaryPath, std::vector<char>& OutShaderBuffer)
{
    // Start our stream at the end of the file, so we can tell how big the file is
    std::ifstream ShaderFileIn(ShaderBinaryPath, std::ios::ate | std::ios::binary);
    if (!ShaderFileIn.is_open())
    {
        std::cerr << "Failed to open shader file: " << ShaderBinaryPath << '\n';
        return false;
    }
    const int ShaderBinarySize = static_cast<int>(ShaderFileIn.tellg());
    // Note that we should call resize instead of reserve here
    // As resize increase both the capacity and size state variables
    // So the vector actually knows it has this much data inside its buffer
    OutShaderBuffer.resize(ShaderBinarySize);
    ShaderFileIn.seekg(0);
    ShaderFileIn.read(OutShaderBuffer.data(), ShaderBinarySize);
    ShaderFileIn.close();
    return true;

}

bool VulkanManager::CreateShaderModule(const std::vector<char>& InShaderBuffer, VkShaderModule& OutShaderModule)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = InShaderBuffer.size();
    // Need to cast because vulkan expects the SPIR-V as an array of 32-bit words
    ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(InShaderBuffer.data());

    OutShaderModule = nullptr;
    if (vkCreateShaderModule(m_VulkanContext.VulkanDevice, &ShaderModuleCreateInfo, nullptr,
        &OutShaderModule) != VK_SUCCESS)
    {
        std::cerr << "Failed to create shader module!" << '\n';
        return false;
    }
    return true;
}

void VulkanManager::RecordFrameCommandBuffer(VkCommandBuffer CommandBuffer, uint32_t ImageIndex,
    uint32_t InstanceCount, VkBuffer VertexBuffer, VkBuffer InstanceBuffer, const PushConstantType &PushConstantData,
    const Commons::Layout::ViewportRect& Viewport)
{
    using namespace Commons;
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {};
    CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    /*
     * Pass A: Render all particles onto the offscreen buffer/target
     */
    VkRenderPassBeginInfo RenderPassBeginInfo = {};
    RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo.renderPass = m_VulkanContext.OffScreen.RenderPass;
    RenderPassBeginInfo.framebuffer = m_VulkanContext.OffScreen.Framebuffer;
    RenderPassBeginInfo.renderArea.offset = { 0, 0 };
    RenderPassBeginInfo.renderArea.extent = {m_VulkanContext.OffScreen.Width, m_VulkanContext.OffScreen.Height};
    RenderPassBeginInfo.clearValueCount = 1;
    VkClearValue ClearColor{};
    ClearColor.color = { {0.05f, 0.05f, 0.05f, 1.f }};
    RenderPassBeginInfo.pClearValues = &ClearColor;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    /* The viewport and scissor are set to the viewport-proportion
     * region of the offscreen target. This ensures particles only
     * render in the area that will be blitted to the swapchain viewport.
     *
     * The offscreen target is 2560×1440 but the viewport might only
     * cover a portion of it (when the panel is open: 1580/1920 * 2560).
     */
    const float ScaleX = static_cast<float>(m_VulkanContext.OffScreen.Width) / Layout::WINDOW_WIDTH;
    const float ScaleY = static_cast<float>(m_VulkanContext.OffScreen.Height) / Layout::WINDOW_HEIGHT;
    VkViewport ParticleVkViewport = {};
    ParticleVkViewport.x = Viewport.X * ScaleX;
    ParticleVkViewport.y = Viewport.Y * ScaleY;
    ParticleVkViewport.width = Viewport.Width * ScaleX;
    ParticleVkViewport.height = Viewport.Height * ScaleY;
    ParticleVkViewport.minDepth = 0.f;
    ParticleVkViewport.maxDepth = 1.f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &ParticleVkViewport);
    VkRect2D VkScissor = {};
    VkScissor.offset = { static_cast<int>(ParticleVkViewport.x), static_cast<int>(ParticleVkViewport.y) };
    VkScissor.extent = { static_cast<uint32_t>(Viewport.Width * ScaleX),
        static_cast<uint32_t>(Viewport.Height * ScaleY) };

    vkCmdSetScissor(CommandBuffer, 0, 1, &VkScissor);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VulkanContext.GraphicsPipeline);

    vkCmdPushConstants(CommandBuffer, m_VulkanContext.GraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
        sizeof(PushConstantType), &PushConstantData);

    VkBuffer VertexInstanceBuffers[] = {VertexBuffer, InstanceBuffer};
    VkDeviceSize Offsets[] = {0, 0};
    vkCmdBindVertexBuffers(CommandBuffer, 0, 2, &VertexBuffer, Offsets);
    // At this point you might tempting to add an index buffer
    vkCmdDraw(CommandBuffer, 6, InstanceCount, 0, 0);

    // At this point, we had set the offscreen target/buffer to TRANSFER_SRC_OPTIMAL
    vkCmdEndRenderPass(CommandBuffer);

    /*
     * Blit: Offscreen buffer -> Swapchain viewport
     */
    VkImageMemoryBarrier ImageMemoryBarrier = {};
    ImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ImageMemoryBarrier.image = m_VulkanContext.SwapChainImages[ImageIndex];
    ImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    ImageMemoryBarrier.subresourceRange.levelCount = 1;
    ImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    ImageMemoryBarrier.subresourceRange.layerCount = 1;
    ImageMemoryBarrier.srcAccessMask = 0;
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0,
        nullptr, 1, &ImageMemoryBarrier);

    /* Clear the entire swapchain image to black before blitting.
     * The blit only covers the viewport region — without this clear,
     * the panel and status bar regions would contain undefined data
     * (because we transitioned from UNDEFINED, discarding old contents)
     */
    VkClearColorValue ClearColorValue = { {0.0f, 0.0f, 0.0f, 1.0f} };
    VkImageSubresourceRange ClearRange = {};
    ClearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ClearRange.baseMipLevel = 0;
    ClearRange.levelCount = 1;
    ClearRange.baseArrayLayer = 0;
    ClearRange.layerCount = 1;
    vkCmdClearColorImage(CommandBuffer, m_VulkanContext.SwapChainImages[ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &ClearColorValue, 1, &ClearRange);

    /* Compute blit regions: offscreen viewport → swapchain viewport
     * The blit maps the viewport-proportional region of the offscreen
     * image to the viewport region of the swapchain image.
     * The panel/status bar regions of the swapchain are NOT touched.
     */
    const float BlitScaleX = static_cast<float>(m_VulkanContext.SwapChainExtent.width) / Layout::WINDOW_WIDTH;
    const float BlitScaleY = static_cast<float>(m_VulkanContext.SwapChainExtent.height) / Layout::WINDOW_HEIGHT;
    VkImageBlit BlitRegion = {};
    BlitRegion.srcOffsets[0] = { 0, 0 };
    BlitRegion.srcOffsets[1] = { static_cast<int>(ParticleVkViewport.width * BlitScaleX),
        static_cast<int>(ParticleVkViewport.height * BlitScaleY) };
    BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    BlitRegion.srcSubresource.mipLevel = 0;
    BlitRegion.srcSubresource.baseArrayLayer = 0;
    BlitRegion.srcSubresource.layerCount = 1;
    BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    BlitRegion.dstSubresource.mipLevel = 0;
    BlitRegion.dstSubresource.baseArrayLayer = 0;
    BlitRegion.dstSubresource.layerCount = 1;
    BlitRegion.dstOffsets[0] = { static_cast<int>(Viewport.X),
        static_cast<int>(Viewport.Y), 0 };
    BlitRegion.dstOffsets[1] = { static_cast<int>(Viewport.X + Viewport.Width * BlitScaleX),
        static_cast<int>(Viewport.Y + Viewport.Height * BlitScaleY), 1 };
    vkCmdBlitImage(CommandBuffer, m_VulkanContext.OffScreen.VkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_VulkanContext.SwapChainImages[ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &BlitRegion, VK_FILTER_LINEAR);

    // Transition swapchain: TRANSFER_DST → COLOR_ATTACHMENT_OPTIMAL
    // (ready for Render Pass B / ImGui)
    ImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr,
        0, nullptr, 1, &ImageMemoryBarrier);

    // PASS B: ImGui → Swapchain (1920×1080)
    VkRenderPassBeginInfo RenderPassBeginInfo_B = {};
    RenderPassBeginInfo_B.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RenderPassBeginInfo_B.renderPass = m_VulkanContext.SwapChainRenderPass;
    RenderPassBeginInfo_B.framebuffer = m_VulkanContext.FrameBuffers[ImageIndex];
    RenderPassBeginInfo_B.renderArea.offset = {0, 0};
    RenderPassBeginInfo_B.renderArea.extent = m_VulkanContext.SwapChainExtent;
    RenderPassBeginInfo_B.clearValueCount = 0; // Remember we are NOT clearing the entire swapchain(which contains our imgui UI!)
    RenderPassBeginInfo_B.pClearValues = nullptr;

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo_B, VK_SUBPASS_CONTENTS_INLINE);

    // Full-window viewport/scissor for ImGui
    VkViewport FullWindowViewport{};
    FullWindowViewport.width = static_cast<float>(m_VulkanContext.SwapChainExtent.width);
    FullWindowViewport.height = static_cast<float>(m_VulkanContext.SwapChainExtent.height);
    FullWindowViewport.minDepth = 0.f;
    FullWindowViewport.maxDepth = 1.f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &FullWindowViewport);

    VkRect2D FullWindowScissor{};
    FullWindowScissor.extent = m_VulkanContext.SwapChainExtent;
    vkCmdSetScissor(CommandBuffer, 0, 1, &FullWindowScissor);

    // ImGui draws here — its widgets are positioned in the panel/status
    // bar regions by the ImGui tutorial code. It draws on top of the
    // clear color in those regions, and does NOT touch the viewport
    // region where blitted particles sit.
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), CommandBuffer);
    // Synchronization Point - Swapchain image now transistions to PRESENT_SRC_KHR
    vkCmdEndRenderPass(CommandBuffer);
    vkEndCommandBuffer(CommandBuffer);
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
 *  GENERAL     — Unrelated to spec or performance
 *  VALIDATION  — Violates the Vulkan specification
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
