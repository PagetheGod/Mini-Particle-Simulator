//
// Created by YWvin on 2026/3/23.
//

//External libs and STL
#include <iostream>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <format>
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
#include <functional>
//Own headers
#include "Application.hpp"
#include "HardwareRenderer.hpp"
#include "Commons.hpp"
#include "ParticleManager.hpp"
Application::Application() : m_Window(nullptr), m_RendererType(RendererType::Software),
m_SoftwareRenderer(nullptr), m_HardwareRenderer(nullptr), m_UIManager(nullptr), m_ParticleManager(nullptr),
m_InputManager(InputManager())
{

}

Application::~Application()
{
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

bool Application::Initialize() {
	bool Result = false;
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return Result;
	}

	// Set up the flags to create our window
	// Last flag handles high-DPI displays so our drawable area matches the pixel counts
	constexpr SDL_WindowFlags WindowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	m_Window = SDL_CreateWindow(Commons::Constants::WINDOW_TITLE, Commons::Layout::WINDOW_WIDTH, Commons::Layout::WINDOW_HEIGHT,
		WindowFlags);
	if (!m_Window)
	{
		std::cerr << "Could not create window! SDL_Error: " << SDL_GetError() << std::endl;
		return Result;
	}

	// Critical for vulkan, since vulkan's swap chain cares about the real pixel counts, not window size
	int DrawableWidth = 0;
	int DrawableHeight = 0;
	SDL_GetWindowSize(m_Window, &DrawableWidth, &DrawableHeight);


	// Get the vulkan instance extensions that SDL needs for creating a surface on our platform
	// It handles cross-platform automatically
	uint32_t ExtensionCount = 0;
	const char* const* Extensions = SDL_Vulkan_GetInstanceExtensions(&ExtensionCount);
	if (!Extensions)
	{
		std::cerr << "Failed to get Vulkan extensions: " << SDL_GetError() << '\n';
		SDL_DestroyWindow(m_Window);
		return Result;
	}
	if (!ShowStartupDialog())
	{
		return false;
	}
	// Init UI manager
	m_UIManager = std::make_unique<UIManager>([this](const bool SetLoopEnable)
	{
		this->m_ShouldLoop = SetLoopEnable;
	});
    return true;
}

bool Application::Frame(const DeltaTimeData& InDeltaTimeData, const InputResult& Input)
{
	ParticleSimulatorConfig ParticleConfig;

	/*
	 * Frame flow:
	 * 1. Update FPS number - Done
	 * 2. Check if settings panel is collapsed, if it's, set the state in UIManager and update camera look at
	 * 3. Call UIFrame(), this will update the UI and take care panel collapse/expansion
	 * 4. Check if user did camera orbiting, if they did, update camera position and orientation
	 * 5. Call ParticleFrame(), this will both spawn and update the particles
	 * 6. Call RenderFrame(), passing it the camera object so the renderer has access to up-to-date view and projection matrix
	 */
	if (Input.Event == InputEvent::ToggleViewport)
	{
		m_UIManager->TogglePanelOpen();
	}
	m_UIManager->UIFrame(InDeltaTimeData, ParticleConfig, m_ParticleManager->GetParticleCount());

}

bool Application::ShowStartupDialog()
{
	// ── Step 1: Create a temporary SDL_Renderer ──
	// This is the software renderer. It's trivial to create and
	// works on every platform without any GPU setup.
	// Got a bit of chicken and egg issue here, no renderer before the start up, requires a renderer to show the start up
	// So we just create the start up using a temporary SDL Renderer, clean it up after we are done
	SDL_Renderer* Renderer = SDL_CreateRenderer(m_Window, nullptr);
	if (!Renderer)
	{
		std::printf("Failed to create renderer for startup dialog: %s\n",
			SDL_GetError());
		// If we can't even create a software renderer, default to software
		// mode (it will fail later too, but at least we tried).
		m_RendererType = RendererType::Software;
		return false;
	}

	SDL_SetRenderVSync(Renderer, 1);

	// ── Step 2: Initialize ImGui with SDL_Renderer backend ──
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& ImGuiIO = ImGui::GetIO();
	// Disable imgui.ini file — we don't want to save layout state
	// for a temporary dialog.
	ImGuiIO.IniFilename = nullptr;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForSDLRenderer(m_Window, Renderer);
	ImGui_ImplSDLRenderer3_Init(Renderer);

	// ── Step 3: Run the modal event loop ──
	RendererType ChosenType = RendererType::Software;
	bool IsDialogOpen = true;

	while (IsDialogOpen)
	{
		// Poll events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
		    ImGui_ImplSDL3_ProcessEvent(&event);
		    if (event.type == SDL_EVENT_QUIT)
		    {
		        // User closed the window during the dialog — just
		        // pick software and let the main loop handle the quit.
		        IsDialogOpen = false;
		    }
		}

		// Start ImGui frame with the weird trio
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		// ── Draw the modal dialog ──
		// We use a fullscreen transparent window as a backdrop, then
		// open a centered modal popup on top of it.
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGuiIO.DisplaySize);
		ImGui::Begin("##backdrop", nullptr,
		             ImGuiWindowFlags_NoTitleBar |
		             ImGuiWindowFlags_NoResize |
		             ImGuiWindowFlags_NoMove |
		             ImGuiWindowFlags_NoScrollbar |
		             ImGuiWindowFlags_NoBackground);

		// Center the popup
		ImVec2 center = ImVec2(ImGuiIO.DisplaySize.x * 0.5f,
		                       ImGuiIO.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("Renderer Selection",
		                           nullptr,
		                           ImGuiWindowFlags_AlwaysAutoResize |
		                           ImGuiWindowFlags_NoMove))
		{
		    ImGui::Text("Please choose a renderer type:");
		    ImGui::Spacing();
		    ImGui::Separator();
		    ImGui::Spacing();

		    ImGui::Text("Software Renderer (SDL3 built in)");
		    ImGui::BulletText("Uses CPU for particle rendering");
		    ImGui::BulletText("Handles ~10K-30K particles at 60 FPS");
		    ImGui::BulletText("Works on any machine, no GPU required");
		    ImGui::Spacing();

			// Creates a button that has the label "Select Software" and a size of 280 x 40
			// Again, ImGUI is an immediate mode GUI library, meaning that it handles rendering and input in a single frame
			// Which is why we have this if check here to see if it got clicked
		    if (ImGui::Button("Select Software", ImVec2(280, 40)))
		    {
		        ChosenType = RendererType::Software;
		        IsDialogOpen = false;
		        ImGui::CloseCurrentPopup();
		    }

		    ImGui::Spacing();
		    ImGui::Separator();
		    ImGui::Spacing();

		    ImGui::Text("GPU Renderer (Vulkan)");
		    ImGui::BulletText("Uses GPU for particle rendering");
		    ImGui::BulletText("Handles ~50K-200K particles at 60 FPS");
		    ImGui::BulletText("Requires a Vulkan-capable GPU");
		    ImGui::Spacing();

		    if (ImGui::Button("Select GPU", ImVec2(280, 40)))
		    {
		        ChosenType = RendererType::Hardware;
		        IsDialogOpen = false;
		        ImGui::CloseCurrentPopup();
		    }

		    ImGui::EndPopup();
		}
		else
		{
		    // The popup wasn't open yet — open it on the first frame.
		    // OpenPopup must be called OUTSIDE of BeginPopupModal.
		    ImGui::OpenPopup("Renderer Selection");
		}

		ImGui::End();  // backdrop window

		// Render
		ImGui::Render();
		SDL_SetRenderDrawColor(Renderer, 25, 25, 30, 255);
		SDL_RenderClear(Renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), Renderer);
		SDL_RenderPresent(Renderer);
	}
	// ── Step 4: Clean up the temporary ImGui + renderer ──
	// We destroy everything here. The main application will re-initialize
	// ImGui with the chosen backend.
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer(Renderer);
	return true;
}
float Application::GetDeltaTime(uint64_t& LastNs)
{
	float DeltaTime = 0.f;

	//Calculate delta time
	const uint64_t CurrentNs = SDL_GetTicksNS();
	const uint64_t ElapsedNs = CurrentNs - LastNs;
	LastNs = CurrentNs;

	//As suggested by the names, times were in nano seconds, gotta convert to seconds
	DeltaTime = static_cast<float>(ElapsedNs) / Commons::Constants::NS_PER_SECOND;

	//Cap it as we said before so things don't explode when we hanged/slowed down
	DeltaTime = DeltaTime > Commons::Constants::MAX_DELTA_TIME ? Commons::Constants::MAX_DELTA_TIME : DeltaTime;

	return DeltaTime;
}

void Application::FrameTiming(DeltaTimeData& DTData)
{
	/*
	 * Make all the frame timing variables static so they persist between frames
	 * Note not ALL of frame timing variables(such as delta time) have to persist
	 * But the following three should
	 */
	static uint64_t LastNs = SDL_GetTicksNS();
	static int FrameCount = 0;
	static float FPSTimer = 0.f;

	DTData.DeltaTime = GetDeltaTime(LastNs);
	DTData.FrameTime = DTData.DeltaTime * 1000.f;

	FrameCount++;
	FPSTimer += DTData.DeltaTime;
	// We passed 1 second, tally up the fps number and fill the struct
	// Then reset the timer
	if (FPSTimer >= 1.f)
	{
		DTData.FPS = FrameCount;
		FrameCount = 0;
		FPSTimer -= 1.f;
	}
}

void Application::Run()
{
	/*
	 * The function that starts, and maintain the main loop of the application
	 * It does the following every frame:
	 * 1. Call input processing function. Check the results.
	 * 2. Skip the frame and quit if user chose to exit, skip to the next frame if user chose to pause
	 * 3. Inform the ui manager if the user pressed tab to toggle the settings panel
	 * 4. Once we are done with processing the above "special" input events, proceed to call Frame()
	 * It also sends the frame function a delta time and input result struct which packs input informations
	 */
	bool Running = true;
	DeltaTimeData DTData{};
	while (Running)
	{
		FrameTiming(DTData);
		InputResult Result = m_InputManager.ProcessInput(m_UIManager->IsPanelOpen());
		if (Result.Event == InputEvent::Quit)
		{
			Running = false;
			// We can just get rid of the Running bool and break here
			continue;
		}
		// Toggle pause
		if (Result.Event == InputEvent::TogglePause)
		{
			m_Paused = !m_Paused;
		}
		// Toggle settings panel
		if (Result.Event == InputEvent::ToggleViewport)
		{
			m_UIManager->TogglePanelOpen();
		}
		Frame(DTData, Result);
	}
}

