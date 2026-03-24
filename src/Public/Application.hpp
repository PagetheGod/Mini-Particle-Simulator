#pragma once
#include "SDL3/SDL_video.h"
#include <memory>
#include "SoftwareRenderer.hpp"

/*
* This class will handle the GUI application, including:
* 1. Initialization and init error handling. It will set up both ImGUI and SDL3, and Vulkan backend(optional)
* 2. Run the main loop - poll events, draw the immediate mode UI for ImGUI, and issue draw calls
* 3. Initialize thread pool, fill the particle object pool
*/

enum class RendererType
{
    Software,
    Hardware
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
    bool Frame();

// Separate access modifiers to help organize functions and variables
public:


private:
    // Shows a modal dialog and returns the user's renderer choice.
    // This function creates a temporary SDL_Renderer, runs a mini event
    // loop with ImGui, and cleans up before returning.
    bool ShowStartupDialog();



    // Get delta time
    float GetDeltaTime(uint64_t& LastNs);
    // Poll application events, this includes inputs, quit, and resizing(optional)
    // This function will return
private:
    SDL_Window* m_Window;
    RendererType m_RendererType;
    std::unique_ptr<SoftwareRenderer> m_SoftwareRenderer;

    //States
    bool m_Running = false;
    bool m_Paused = false;
    bool m_IsPanelOpen = true;
};


