//
// Created by YWvin on 2026/3/24.
//

//External libs and STL
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <iostream>
#include <string>
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_render.h"
#include "glm/exponential.hpp"
//Own headers
#include "SoftwareRenderer.hpp"
#include "Commons.hpp"
#include "Camera2D.hpp"



SoftwareRenderer::SoftwareRenderer(ParticleManager* InParticleManagerPtr) : m_Renderer(nullptr), m_Window(nullptr),
m_ParticleTexture(nullptr), m_ParticleManager(InParticleManagerPtr), m_Camera(nullptr)
{

}

SoftwareRenderer::~SoftwareRenderer()
{
    // Clean up
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_ParticleTexture)
    {
        SDL_DestroyTexture(m_ParticleTexture);
    }
    if (m_Renderer)
    {
        SDL_DestroyRenderer(m_Renderer);
    }
    m_ParticleTexture = nullptr;
    m_Renderer = nullptr;
}

bool SoftwareRenderer::Initialize(SDL_Window* Window)
{
    /* Create the SDL_Renderer
     * SDL_CreateRenderer picks the best available backend automatically:
     * On Windows is DX11/12
     * On MacOS is Metal
     * On Linux is OpenGL
     * Despite using GPU-accelerated compositing, the PARTICLE RENDERING
     * is still CPU-based (SDL_RenderPoint / SDL_RenderGeometry).
     * ImGui rendering is handled by the SDL_Renderer too.
     */
    m_Window = Window;
    m_Renderer = SDL_CreateRenderer(m_Window, nullptr);
    SDL_SetRenderVSync(m_Renderer, 1);

    if (!m_Renderer)
    {
        std::cerr << "Failed to create SDL_Renderer: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_SetRenderVSync(m_Renderer, 1);

    /*
     * Added this to resolve weird size issues with Retina display
     * From what I understand, basically this is a DPI-aware issues
     * Most of the time when we work with drawing things to a screen, we deal with
     * logical pixels(or points), the OS then scale them behind the scene to map
     * them to real pixels, this abstracts things away and make our life easier
     * However, in our case, we asked for High pixel density in app init because our fonts
     * needed to look sharp and elligible, this means we have to work with real pixels now
     * And on mac, the 1920 x 1080 window has way more pixels because it's Retina
     * So we are drawing 1920 x 1080 pixels to a window that needs a much larger frame buffer
     *
     * This call basically resets us to drawing to logical points, then SDL will just expand
     * our buffer to fit the drawable area
     * Letterbox just tells SDL to not stretch things when user resizes
     */
    SDL_SetRenderLogicalPresentation(m_Renderer,
        Commons::Layout::WINDOW_WIDTH, Commons::Layout::WINDOW_HEIGHT,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Initialize ImGui. We are not setting the IniFileName to nullptr because in this case
    // we do want to save the layout state since this is a persistent ui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& ImGuiIO = ImGui::GetIO();
    ImGuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; //Enable full keyboard inputs(enter, tab, space, etc)

    ImGui::StyleColorsDark();

    // Load Roboto font at the correct size for the display's DPI scale.
    // Base size is 24 (1.5x the original 16) for readability - the stock
    // ImGui default ends up too small on modern high-res monitors.
    //
    // On HiDPI (Retina) we rasterize at BaseFontSize * DpiScale = 48px so the
    // glyph texture has the extra detail, then set FontGlobalScale = 1/DpiScale
    // so ImGui draws the glyphs at their base 24-unit size in the logical
    // canvas. Combined with SDL_SetRenderLogicalPresentation above, the 48px
    // font texture maps 1:1 to physical pixels on a 2x display - pixel-perfect
    // crisp text without blurry upscaling.
    //
    // SDL_GetBasePath() resolves to the directory containing the executable
    // (where the CMake post-build step copies Roboto-Medium.ttf). A relative
    // path would fail when the binary is invoked from the project root
    // (e.g., `make run`), leaving ImGui's atlas empty and making every
    // subsequent draw render against an invalid font texture.
    constexpr float BaseFontSize = 24.f;
    const float DpiScale = SDL_GetWindowDisplayScale(m_Window);
    const char* BasePath = SDL_GetBasePath();
    const std::string FontPath = std::string(BasePath ? BasePath : "") + "Roboto-Medium.ttf";
    ImFont* LoadedFont = ImGuiIO.Fonts->AddFontFromFileTTF(FontPath.c_str(), BaseFontSize * DpiScale);
    if (LoadedFont == nullptr)
    {
        std::cerr << "Failed to load font from '" << FontPath
                  << "'. Falling back to ImGui default font." << std::endl;
        ImGuiIO.Fonts->AddFontDefault();
    }

    // Initialize backends
    // The SDL3 platform backend handles input (mouse, keyboard, clipboard).
    // The SDLRenderer3 backend handles rendering ImGui's draw lists.
    ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer);
    ImGui_ImplSDLRenderer3_Init(m_Renderer);
    // Create the Gaussian particle texture
    m_ParticleTexture = CreateGaussianGlowTexture(PARTICLE_TEXTURE_WIDTH);
    if (!m_ParticleTexture)
    {
        return false;
    }

    m_Camera = std::make_unique<Camera2D>();

    return true;
}

void SoftwareRenderer::RenderFrame(const bool IsPanelOpen)
{
    BeginFrame(IsPanelOpen);
    EndFrame(IsPanelOpen);
}

void SoftwareRenderer::BeginFrame(const bool IsPanelOpen)
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    // This starts a new imgui frame, starting at this point
    // we can submit commands to imgui until we call endframe or render
    ImGui::NewFrame();
}

void SoftwareRenderer::EndFrame(const bool IsPanelOpen)
{
    using namespace Commons;
    // End the current imgui frame, all draw data should be finalized at this point
    ImGui::Render();
    // Set logical resolution to 2560 x 1440 for particle supersampling
    // SDL scales all draw calls to fit the 1920×1080 window.
    SDL_SetRenderLogicalPresentation(m_Renderer, Layout::RENDER_WIDTH, Layout::RENDER_HEIGHT,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);
    // Clear the entire screen with the background color.
    SDL_SetRenderDrawColorFloat(m_Renderer, 0.06f, 0.06f, 0.08f, 1.0f);
    SDL_RenderClear(m_Renderer);

    // Clip particle drawing to the viewport
    // SDL_SetRenderClipRect restricts all subsequent draw calls to
    // the given rectangle. This is the software equivalent of Vulkan's
    // VkScissor(I am not doing that) — particles physically cannot render outside this rect.
    const Layout::ViewportRect VpRect = Layout::GetViewportRect(IsPanelOpen);
    const float ScaleX = Layout::RENDER_WIDTH / VpRect.Width;
    const float ScaleY = Layout::RENDER_HEIGHT / VpRect.Height;

    const SDL_Rect ClipRect = {static_cast<int>(VpRect.X), static_cast<int>(VpRect.Y),
        static_cast<int>(VpRect.Width * ScaleX), static_cast<int>(VpRect.Height * ScaleY)};
    SDL_SetRenderClipRect(m_Renderer, &ClipRect);



    // Draw particles ONLY in the viewport region:
    RenderParticles(VpRect);

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

SDL_Texture* SoftwareRenderer::CreateGaussianGlowTexture(float Diameter) {
    /*
     * This function creates a circle-ish 64x64 texture with soft Gaussian fall off(alpha)
     * So it gives each particle a smoother, glowing appearance
     * instead of just a flat color dot
     * 2D Gaussian function centered at origin (0, 0) without the 1/2Pi * Sigma^2 normalization:
     * f(x, y) = e^(-[(x - 0)^2 + (y - 0)^2)/2*sigma^2)
     *
     */


    // Create a CPU-side surface to fill pixel by pixel
    SDL_Surface* Surface = SDL_CreateSurface(Diameter, Diameter, SDL_PIXELFORMAT_RGBA8888);
    if (!Surface)
    {
        std::cerr << "Failed to create SDL_Surface: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    // Diameter just refers to the length of the sizes of our texture in pixels
    // So in our case it's 64, then the center sits at (32/31, 32/31), center as in the center of our texture
    const float Center = Diameter / 2.f;
    const float Sigma = Diameter / 4.5f;
    const float TwoSigmaSquare = Sigma * Sigma * 2.f;

    uint32_t* Pixels = static_cast<uint32_t*>(Surface->pixels);
    // Pitch is just graphics term for the width of a single row of pixels
    // We explicitly get it here because rows can get padded for memory alignment
    const int Pitch = Surface->pitch / 4;
    for (int i = 0; i < Diameter; i++)
    {
        for (int j = 0; j < Diameter; j++)
        {
            // Add 0.5 to sample pixel center
            float Dx = j - Center + 0.5f;
            float Dy = i - Center + 0.5f;
            float DistSq = Dx * Dx + Dy * Dy;

            // Gaussian falloff, we are 1.0 of the color at the center
            // And 0 of the color at the edges
            float Alpha = glm::exp(-DistSq / TwoSigmaSquare);

            // Rejection sampling, discard any thing outside the circle
            if (DistSq > Center * Center)
            {
                Alpha = 0.f;
            }
            uint8_t FinalAlpha = static_cast<uint8_t>(Alpha * 255.f);

            // The pixel format is A8B8G8R8, so alpha is in the highest byte
            // The reason why we use 0xFF(all whites) is so that we just scale this with w/e color we got later
            // So we reuse this texture
            Pixels[i * Pitch + j] = (FinalAlpha << 24) | 0x00FFFFFF;
        }
    }
    // Upload the CPU surface to a GPU texture
    SDL_Texture* Texture = SDL_CreateTextureFromSurface(m_Renderer, Surface);
    SDL_DestroySurface(Surface);
    if (!Texture)
    {
        std::cerr << "Failed to create SDL_Texture: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    // Set texture's blend mode to additive, because we do not want occlusions
    // We want overlapping particles to all contribute to a pixel's coloring
    SDL_SetTextureBlendMode(Texture, SDL_BLENDMODE_ADD);
    return Texture;
}

void SoftwareRenderer::RenderParticles(const Commons::Layout::ViewportRect& VpRect)
{
    using namespace Commons;
    for (uint32_t i = 0; i < m_ParticleManager->GetParticleCount(); i++)
    {
        // Transform particles' world positions into screen positions via 2D camera
        const glm::vec3 WorldPoS = m_ParticleManager->GetParticlePos(i);
        const glm::vec2 ScreenPos = m_Camera->WorldToScreen(WorldPoS.x, WorldPoS.y, VpRect);

        // Scale the particle — multiply by BASE_PARTICLE_SCREEN_SIZE because config
        // scale values (0.1-1.0) were designed for 3D projection, not direct pixel mapping
        const float ScreenSize = m_ParticleManager->GetParticleScale(i) * m_Camera->GetZoom() * BASE_PARTICLE_SCREEN_SIZE;

        // Cull particles that are outside the viewport so we don't send
        // unnecessary draw calls
        if (IsOutsideViewport(ScreenSize, ScreenPos, VpRect))
        {
            continue;
        }
        // Modify our earlier Gaussian texture so it's tinted by particle's colors
        const glm::vec3 ParticleColor = m_ParticleManager->GetParticleColor(i);
        SDL_SetTextureColorModFloat(m_ParticleTexture, ParticleColor.r, ParticleColor.g, ParticleColor.b);

        // Do the same for alpha, we do not store the alpha values explicitly
        // We calculate it using the relative lifetime
        const float ParticleRelLifeTime = m_ParticleManager->GetParticleRelLifeTime(i);
        SDL_SetTextureAlphaModFloat(m_ParticleTexture, ParticleRelLifeTime);

        // Destination rect, it's a rectangle stored in floats
        // I think of it as a representation of the region that our particle covers
        SDL_FRect ParticleDest = {ScreenPos.x - ScreenSize * 0.5f, ScreenPos.y - ScreenSize * 0.5f,
            ScreenSize, ScreenSize};

        // Draw the Gaussian texture onto the region we just defined
        SDL_RenderTexture(m_Renderer, m_ParticleTexture, nullptr, &ParticleDest);
    }
}

bool SoftwareRenderer::IsOutsideViewport(const float ScreenSize,const glm::vec2& InPos,
    const Commons::Layout::ViewportRect& InViewportRect)
{
    return (InPos.x + ScreenSize > InViewportRect.Width) || (InPos.y + ScreenSize > InViewportRect.Height) ||
        (InPos.x - ScreenSize < 0.f) || (InPos.y - ScreenSize < 0.f);
}

