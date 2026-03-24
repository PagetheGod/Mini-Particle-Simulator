//
// Created by YWvin on 2026/3/24.
//

//External libs and STL
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <iostream>
//Own headers
#include "SoftwareRenderer.hpp"
#include "Commons.hpp"


SoftwareRenderer::SoftwareRenderer() : m_Renderer(nullptr), m_Window(nullptr)
{

}

SoftwareRenderer::~SoftwareRenderer()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_Renderer)
    {
        SDL_DestroyRenderer(m_Renderer);
    }
    m_Renderer = nullptr;
}

bool SoftwareRenderer::Initialize(SDL_Window* Window)
{
    // ── Create the SDL_Renderer ──
    // SDL_CreateRenderer picks the best available backend automatically:
    // On Windows is DX11/12
    // On MacOS is Metal
    // On Linux is OpenGL
    // Despite using GPU-accelerated compositing, the PARTICLE RENDERING
    // is still CPU-based (SDL_RenderPoint / SDL_RenderGeometry).
    // ImGui rendering is handled by the SDL_Renderer too.
    m_Renderer = SDL_CreateRenderer(Window, nullptr);
    if (!m_Renderer)
    {
        std::cerr << "Failed to create SDL_Renderer: " << SDL_GetError() << std::endl;
        return false;
    }
    // Initialize ImGui. We are not setting the IniFileName to nullptr because in this case
    // we do want to save the layout state since this is a persistent ui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& ImGuiIO = ImGui::GetIO();
    ImGuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; //Enable full keyboard inputs(enter, tab, space, etc)

    ImGui::StyleColorsDark();

    // Initialize backends
    // The SDL3 platform backend handles input (mouse, keyboard, clipboard).
    // The SDLRenderer3 backend handles rendering ImGui's draw lists.
    ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer3_Init(m_Renderer);

    return true;
}

void SoftwareRenderer::BeginFrame()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    // This starts the a new imgui frame, starting at this point
    // we can submit commands to imgui until we call endframe or render
    ImGui::NewFrame();
}

void SoftwareRenderer::EndFrame()
{
    using namespace Commons;
    // End the current imgui frame, all draw data should be finalized at this point
    ImGui::Render();
    // Set logical resolution to 2560 x 1440 for supersampling
    // Set logical resolution to 2560×1440 for particle drawing.
    // SDL scales all draw calls to fit the 1920×1080 window.
    SDL_SetRenderLogicalPresentation(m_Renderer, Layout::RENDER_WIDTH, Layout::RENDER_HEIGHT,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);
    // Clear the entire screen with the background color.
    SDL_SetRenderDrawColorFloat(m_Renderer, 0.06f, 0.06f, 0.08f, 1.0f);
    SDL_RenderClear(m_Renderer);

    // SPLIT-REGION: Clip particle drawing to the viewport
    // SDL_SetRenderClipRect restricts ALL subsequent draw calls to
    // the given rectangle. This is the software equivalent of Vulkan's
    // VkScissor — particles physically cannot render outside this rect.
    const Layout::ViewportRect VpRect = Layout::GetViewportRect(true);
    const float ScaleX = Layout::RENDER_WIDTH / VpRect.Width;
    const float ScaleY = Layout::RENDER_HEIGHT / VpRect.Height;

    const SDL_Rect ClipRect = {static_cast<int>(VpRect.X), static_cast<int>(VpRect.Y),
        static_cast<int>(VpRect.Width * ScaleX), static_cast<int>(VpRect.Height * ScaleY)};
    SDL_SetRenderClipRect(m_Renderer, &ClipRect);



    // Draw particles ONLY in the viewport region:
    //   draw_particles_software(renderer, particle_system, viewport);

    // Remove the clip rect before drawing ImGui
    // ImGui needs to draw in the panel and status bar regions,
    // which are outside the viewport clip rect.
    SDL_SetRenderClipRect(m_Renderer, nullptr);

    // Renders imgui at window native resolution
    SDL_SetRenderLogicalPresentation(m_Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);


    // Draw ImGui in the panel and status bar regions.
    // ImGui windows are positioned outside the viewport rect,
    // so they don't overlap with particles. The opaque ImGui
    // background covers the cleared region where no particles were drawn.
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

    // Present
    SDL_RenderPresent(m_Renderer);
}


