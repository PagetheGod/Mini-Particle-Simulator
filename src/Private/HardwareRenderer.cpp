
#include <algorithm>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstring>  // for strcmp
#include <iostream>
#include <format>
#include <fstream>
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
// Own headers
#include "HardwareRenderer.hpp"
#include "Camera.hpp"
#include "glm/ext/matrix_transform.hpp"


using namespace Commons;
HardwareRenderer::HardwareRenderer(ParticleManager* InParticleManager) : m_VulkanManager(nullptr), m_Camera(nullptr),
m_ParticleManager(InParticleManager), m_VulkanContextPtr(nullptr), m_Window(nullptr)
{

}

HardwareRenderer::~HardwareRenderer()
{
    // The vulkan device might still be in use here
    // Also there is a chance the destructor gets called after some init failures, so need to check
    if (m_VulkanContextPtr && m_VulkanContextPtr->VulkanDevice)
    {
        vkDeviceWaitIdle(m_VulkanContextPtr->VulkanDevice);
    }
    // Destroy in reversed order
    if (m_VertexBuffer.VulkanBuffer)
    {
        m_VulkanManager->DestroyBuffer(m_VertexBuffer);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    m_VulkanContextPtr = nullptr;
    m_ParticleManager = nullptr;
}

bool HardwareRenderer::Initialize(SDL_Window* InWindow)
{
    bool Result = false;

    m_VulkanManager = std::make_unique<VulkanManager>();
    m_Window = InWindow;
    Result = m_VulkanManager->Initialize(InWindow);
    if (!Result)
    {
        Utility::ShowError("Hardware Renderer", "Failed to initialize the Vulkan manager. See validation-layer output for details.");
        return Result;
    }
    m_Camera = std::make_unique<Camera>();
    m_VulkanContextPtr = m_VulkanManager->GetVulkanContext();
    ImGui_ImplVulkan_InitInfo ImGuiVkInitInfo{};
    ImGuiVkInitInfo.Device = m_VulkanContextPtr->VulkanDevice;
    ImGuiVkInitInfo.Instance = m_VulkanContextPtr->VulkanInstance;
    ImGuiVkInitInfo.PhysicalDevice = m_VulkanContextPtr->VulkanPhysicalDevice;
    ImGuiVkInitInfo.Allocator = nullptr;
    ImGuiVkInitInfo.DescriptorPool = nullptr;
    ImGuiVkInitInfo.DescriptorPoolSize = 16;
    ImGuiVkInitInfo.ApiVersion = VK_API_VERSION_1_4;
    ImGuiVkInitInfo.Queue = m_VulkanContextPtr->GraphicsQueue;
    ImGuiVkInitInfo.QueueFamily = m_VulkanContextPtr->GraphicsFamily;
    ImGuiVkInitInfo.MinImageCount = m_VulkanContextPtr->MinImageCount;
    ImGuiVkInitInfo.ImageCount = static_cast<uint32_t>(m_VulkanContextPtr->SwapChainImages.size());
    ImGuiVkInitInfo.PipelineInfoMain.RenderPass = m_VulkanContextPtr->SwapChainRenderPass;// Not the offscreen particle pass
    ImGuiVkInitInfo.PipelineInfoMain.Subpass = 0;
    ImGuiVkInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGuiVkInitInfo.UseDynamicRendering = false;

    Result = ImGui_ImplSDL3_InitForVulkan(m_Window);
    if (!Result)
    {
        Utility::ShowError("Hardware Renderer", "ImGui_ImplSDL3_InitForVulkan() failed.");
        return Result;
    }
    Result = ImGui_ImplVulkan_Init(&ImGuiVkInitInfo);
    if (!Result)
    {
        Utility::ShowError("Hardware Renderer", "ImGui_ImplVulkan_Init() failed.");
        return Result;
    }
    // Create the persistent vertex buffer containing the six quad vertices
    m_VertexBuffer = m_VulkanManager->CreateBufferWithData(QuadVertices, sizeof(QuadVertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (m_VertexBuffer.VulkanBuffer == nullptr)
    {
        Utility::ShowError("Hardware Renderer", "Failed to create the quad vertex buffer.");
        return Result;
    }
    return Result;
}

void HardwareRenderer::BeginFrame()
{
    // Same thing with the software renderer, we need to start an ImGui frame no matter what
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplVulkan_NewFrame();
    // Start the imgui frame
    ImGui::NewFrame();
}

void HardwareRenderer::EndFrame(const bool IsPanelOpen)
{
    // First end the ImGui frame
    ImGui::Render();
    // Build the push constants
    // We are not passing in the world matrix in this case, because it's default to the identity matrix
    PushConstantType PushConstants{};
    PushConstants.CameraRight = m_Camera->GetRight();
    PushConstants.CameraUp = m_Camera->GetUp();
    glm::mat4x4 ViewMatrix;
    glm::mat4x4 ProjectionMatrix;
    const Layout::ViewportRect Viewport = Layout::GetViewportRect(IsPanelOpen);
    m_Camera->GetViewMatrix(ViewMatrix);
    m_Camera->GetProjectionMatrix(ProjectionMatrix,
        static_cast<float>(Viewport.Width) / static_cast<float>(Viewport.Height));
    const glm::mat4x4 ViewProjection = ProjectionMatrix * ViewMatrix;
    PushConstants.ViewProjection = glm::transpose(ViewProjection);
    // Upload the particle data, we wait for the fence here to synchronize
    const uint32_t CurrentFrameIndex = m_VulkanContextPtr->CurrentFrame;
    // Needs to wait for GPU to finish its current work
    vkWaitForFences(m_VulkanContextPtr->VulkanDevice, 1, &m_VulkanContextPtr->InFlightFence[CurrentFrameIndex], VK_TRUE,
        UINT64_MAX);
    UploadParticleData(CurrentFrameIndex, *(m_ParticleManager->GetParticleStates()),
        m_ParticleManager->GetParticleCount());
    // Tell Vulkan manager to draw the current frame
    m_VulkanManager->DrawFrame(m_ParticleManager->GetParticleCount(), m_VertexBuffer, PushConstants,
        Viewport);
}


void HardwareRenderer::UploadParticleData(uint32_t CurrentFrame, const ParticleStates& InParticleStates,
                                          uint32_t ParticleCount)
{
    ParticleInstanceBuffers& BufferSet = m_VulkanContextPtr->ParticleInstanceBufferArray[CurrentFrame];

    const size_t BytesToCopy = ParticleCount * 4;

    /*
     * Source/destination pairs for each attribute. The order must match the Mapped[] indices set up in
     * CreateParticleInstanceBuffers - 0...8 = Px, Py, Pz, R, G, B, Size, LifeTime, MaxLifeTime
     */
    const float* Sources[9] = {
        InParticleStates.Px.get(),
        InParticleStates.Py.get(),
        InParticleStates.Pz.get(),
        InParticleStates.R.get(),
        InParticleStates.G.get(),
        InParticleStates.B.get(),
        InParticleStates.Size.get(),
        InParticleStates.LifeTime.get(),
        InParticleStates.MaxLifeTime.get()
    };

    // Copy all the data to VRAM through persistently mapped GPU ptr

    /*
     * We do not have to call vkFlushMappedMemoryRanges because we allocated the buffers as
     * HOST_COHERENT, which means that any change to the buffers by the CPU will make these writes visible
     * to the GPU "automatically". By the nex vkQueueSubmit
     * We also do not need memory barrier, Vulkan's host-to-device write visibility is handled implicitly by
     * queue submissions. Storage buffer reads in the vertex shader of this frame's command buffer (not recorded
     * at this step of the frame) will see the data we just wrote
     */
    for (uint32_t i = 0; i < 9; i++)
    {
        memcpy(BufferSet.MappedPtr[i], Sources[i], BytesToCopy);
    }

}


