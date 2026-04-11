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
    SoftwareRenderer(ParticleManager* InParticleManagerPtr);
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
    void EndFrame(const bool IsPanelOpen);

    // Getters and setters
    [[nodiscard]] Camera2D* GetCamera()
    {
        // This is not marked const because whoever gets this can modify camera 2D members
        return m_Camera.get();
    }
private:
    SDL_Texture* CreateGaussianGlowTexture(float Diameter);
    void RenderParticles(const Commons::Layout::ViewportRect& VpRect);
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
    // World-scale to pixel-scale multiplier — a config scale of 1.0 renders as this many pixels
    static constexpr float BASE_PARTICLE_SCREEN_SIZE = 2.f;
};
