#pragma once

// External libs and STL
#include <SDL3/SDL.h>

// Own headers
#include "Camera2D.hpp"
#include "ParticleManager.hpp"



// This class implements the rendering logics by using SDL3's built in software renderer
class SoftwareRenderer
{
public:
    // Constructors and destructor
    SoftwareRenderer();
    SoftwareRenderer(const SoftwareRenderer&) = delete;
    SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;
    SoftwareRenderer(SoftwareRenderer&&) = delete;
    SoftwareRenderer& operator=(SoftwareRenderer&&) = delete;
    ~SoftwareRenderer();

    // Actual work functions
    // Initialize the software renderer and ImGui backend.
    // Returns true on success.
    bool Initialize(SDL_Window *window);

    void RenderFrame(const bool IsPanelOpen);


    // Called at the start of each frame, before any ImGui calls.

    void BeginFrame(const bool IsPanelOpen);

    // Called after ImGui::Render(), draws ImGui + clears the background.
    void EndFrame();

private:
    SDL_Texture* CreateGaussianGlowTexture(float Diameter);
    void RenderParticles(const bool IsPanelOpen);
    bool IsOutsideViewport(const float ScreenSize, const glm::vec2& InPos, const Commons::Layout::ViewportRect&
        InViewportRect);
private:
    SDL_Renderer* m_Renderer;
    SDL_Window* m_Window;
    SDL_Texture* m_ParticleTexture;
    ParticleManager* m_ParticleManager;
    std::unique_ptr<Camera2D> m_Camera;

    // Constants
    static constexpr float PARTICLE_TEXTURE_WIDTH = 64;
    static constexpr float PARTICLE_TEXTURE_HEIGHT = 64;
};
