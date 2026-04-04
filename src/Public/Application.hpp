#pragma once
#include "SDL3/SDL_video.h"
#include <memory>

#include "InputManager.hpp"
#include "SoftwareRenderer.hpp"
#include "UIManager.hpp"

/*
* This class will handle the GUI application, including:
* 1. Initialization and init error handling. It will set up both ImGUI and SDL3, and Vulkan backend(optional)
* 2. Run the main loop - poll events, draw the immediate mode UI for ImGUI, and issue draw calls
* 3. Initialize thread pool, fill the particle object pool
*/

class ParticleManager;
class HardwareRenderer;
class UIManager;

enum class RendererType : uint8_t
{
    Software,
    Hardware
};

/*
 * Enum to help us transition between different states in the application
 * Emitting: the emitter is still spawning particles, this is only really relevant to a continuous emitter
 * Fading: the emitter had stopped spawning particles, but there are still live particles to render
 * Stopped: either particles had all died out, or the user paused the playback
 */
enum class PlaybackState : uint8_t
{
    Emitting,
    Fading,
    Stopped
};



class Application {
public:
    // Constructors and destructors
    Application();
    Application(const Application&) = delete; //Makes no sense to copy/move the application instance in our case
    Application(Application&&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application();

    //Init and frame function
    bool Initialize();
    void Run();
    bool Frame(const DeltaTimeData& InDeltaTimeData, const InputResult& Input);

// Separate access modifiers to help organize functions and variables
public:


private:
    // Shows a modal dialog and returns the user's renderer choice.
    // This function creates a temporary SDL_Renderer, runs a mini event
    // loop with ImGui, and cleans up before returning.
    bool ShowStartupDialog();


    // Get delta time
    float GetDeltaTime(uint64_t& LastNs);
    // Calculate delta time, FPS, and frame time each frame
    void FrameTiming(DeltaTimeData& DTData);
    // Poll application events, this includes inputs, quit, and resizing(optional)
    // This function will return
private:
    SDL_Window* m_Window;
    RendererType m_RendererType;
    std::unique_ptr<SoftwareRenderer> m_SoftwareRenderer;
    std::unique_ptr<HardwareRenderer> m_HardwareRenderer;
    std::unique_ptr<UIManager> m_UIManager;
    std::unique_ptr<ParticleManager> m_ParticleManager;
    InputManager m_InputManager;
    //States
    bool m_Running = false;
    bool m_Paused = false;
    bool m_IsLMBPressed = false;
    bool m_IsPanelOpen = true;
    bool m_ShouldLoop = true;
    PlaybackState m_PlaybackState = PlaybackState::Stopped;
    float m_PlaybackLeft = 0.f;
    bool m_IsConfigDirty = false;
    // Constants
    // How long would we play the entire scene until we stop or loop
    static constexpr float PLAYBACK_DURATION = 5.f;
};


