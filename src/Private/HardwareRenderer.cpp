#include "HardwareRenderer.hpp"

#include <algorithm>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstring>  // for strcmp
#include <limits>
#include <iostream>
#include <format>
#include <set>


HardwareRenderer::HardwareRenderer(){
}

HardwareRenderer::~HardwareRenderer() {
}

bool HardwareRenderer::Initialize()
{

}



bool HardwareRenderer::CreateInstance()
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

bool HardwareRenderer::SetupDebugMessenger()
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
    auto Func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_VulkanContext.VulkanInstance,
        "vkCreateDebugUtilsMessengerEXT");
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

bool HardwareRenderer::CreateSurface()
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

bool HardwareRenderer::PickPhysicalDevice()
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

bool HardwareRenderer::CreateLogicalDevice()
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


bool HardwareRenderer::CreateSwapChain()
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

bool HardwareRenderer::CreateOffscreenTarget(uint32_t Width, uint32_t Height) {
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

bool HardwareRenderer::CreateOffScreenRenderPass()
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

bool HardwareRenderer::CreateSwapChainRenderPass()
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

bool HardwareRenderer::CreateFrameBuffers()
{
    m_VulkanContext.FrameBuffers.reserve(m_VulkanContext.SwapChainImages.size());
    for (size_t i = 0; i < m_VulkanContext.SwapChainImages.size(); i++)
    {

    }
}

bool HardwareRenderer::CreateCommandPool() {
}

bool HardwareRenderer::AllocateCommandBuffers() {
}

bool HardwareRenderer::CreateSyncObjects() {
}

bool HardwareRenderer::CleanUpContext() {
}

void HardwareRenderer::DestoryOffscreenTarget()
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

uint32_t HardwareRenderer::FindMemoryType(VkPhysicalDevice PhysicalDevice, uint32_t TypeFilter,
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


