//External libs and STL
#include <iostream>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
//Own headers
#include "Application.hpp"

#include "Camera.hpp"
#include "HardwareRenderer.hpp"
#include "Commons.hpp"
#include "ParticleManager.hpp"
using namespace Commons;

Application::Application() : m_Window(nullptr), m_RendererType(RendererType::Software),
m_SoftwareRenderer(nullptr), m_HardwareRenderer(nullptr), m_UIManager(nullptr), m_ParticleManager(nullptr),
m_InputManager(InputManager()), m_Camera2D(nullptr), m_Camera(nullptr)
{

}

Application::~Application()
{
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}
// For this project we are just printing all errors to cerr
// But there are better ways to show errors, either outputting through a message box
// Or recording them to a log file
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

	// Construct the particle manager
	m_ParticleManager = std::make_unique<ParticleManager>();

	// Create imgui context regardless of which renderer we choose
	ImGui::CreateContext();
	// Set up the UI and font scales for both software and hardware renderer
	SetUIandFontScale();
	// Init the renderer of the user's choice
	if (m_RendererType == RendererType::Software)
	{
		m_SoftwareRenderer = std::make_unique<SoftwareRenderer>(m_ParticleManager.get());
		Result = m_SoftwareRenderer->Initialize(m_Window);
		if (!Result)
		{
			std::cerr << "Failed to initialize software renderer!" << std::endl;
			return Result;
		}
	}
	else
	{
		m_HardwareRenderer = std::make_unique<HardwareRenderer>(m_ParticleManager.get());
		Result = m_HardwareRenderer->Initialize(m_Window);
		if (!Result)
		{
			std::cerr << "Failed to initialize hardware renderer!" << std::endl;
			return Result;
		}
	}

	// Set the camera ptr because we need it to do camera movements
	m_Camera2D = m_SoftwareRenderer->GetCamera();
	// Do the initialization of particle manager last because it requires lots of allocations
	m_ParticleManager->InitializeParticles();
	// Set play back left to max
	m_PlaybackLeft = PLAYBACK_DURATION;
    return true;
}

bool Application::Frame(const DeltaTimeData& InDeltaTimeData, const InputResult& Input)
{
	/*
	 * Frame flow:
	 * 1. BeginFrame: must happen before any ImGui calls (calls ImGui::NewFrame)
	 * 2. Handle input events (toggle panel, camera pan/zoom)
	 * 3. UIFrame: submits all ImGui widgets, returns config dirty flag
	 * 4. ParticleFrame: spawn and update particles (conditional on pause/playback)
	 * 5. EndFrame: renders particles, presents ImGui, swaps buffers
	 * BeginFrame and EndFrame run every frame unconditionally. Only particle
	 * updates are gated by pause/playback state.
	 */
	const bool IsPanelOpen = m_UIManager->IsPanelOpen();

	// BeginFrame starts the ImGui frame, must come before any widget calls
	if (m_RendererType == RendererType::Software)
	{
		m_SoftwareRenderer->BeginFrame();
	}
	else
	{
		m_HardwareRenderer->RenderFrame();
	}

	// Handle input
	if (Input.Event == InputEvent::ToggleViewport)
	{
		m_UIManager->TogglePanelOpen();
		// Viewport size just changed, refresh the camera's pan bounds so we can
		// reach the newly-revealed area (or get clamped back into the new bounds)
		if (m_RendererType == RendererType::Software)
		{
			m_Camera2D->OnViewportResized(Layout::GetViewportRect(m_UIManager->IsPanelOpen()));
		}
		else
		{
			m_Camera->AdjustCameraForResize(Layout::GetViewportRect(IsPanelOpen),
				Layout::GetViewportRect(m_UIManager->IsPanelOpen()));
		}

	}
	if (Input.Event == InputEvent::CameraPan)
	{
		if (m_RendererType == RendererType::Software)
		{
			m_Camera2D->Pan(Input.MouseDelta.x, Input.MouseDelta.y);
		}
		else
		{
			m_Camera->Orbit(Input);
		}

	}
	if (Input.Event == InputEvent::CameraZoom)
	{
		Layout::ViewportRect VpRect = Layout::GetViewportRect(IsPanelOpen);
		m_Camera2D->ZoomAt(Input.ScrollDelta, Input.MousePosition.x, Input.MousePosition.y,
			VpRect);
	}

	// UI, submits ImGui widgets between NewFrame and Render
	// Accumulate dirty state with |= so changes made while paused aren't lost
	// m_IsConfigDirty stays true until ParticleFrame actually processes it
	m_IsConfigDirty |= m_UIManager->UIFrame(InDeltaTimeData, m_ParticleConfig, m_ParticleManager->GetParticleCount(), m_Paused);

	// Particle updates, only when not paused and playback is active
	if (!m_Paused)
	{
		if (m_IsConfigDirty)
		{
			m_PlaybackLeft = PLAYBACK_DURATION - InDeltaTimeData.DeltaTime;
			m_ParticleManager->ParticleFrame(InDeltaTimeData.DeltaTime, m_ParticleConfig, m_IsConfigDirty);
			m_IsConfigDirty = false;
		}
		else if (m_PlaybackLeft > 0.f)
		{
			m_ParticleManager->ParticleFrame(InDeltaTimeData.DeltaTime, m_ParticleConfig, false);
			m_PlaybackLeft -= InDeltaTimeData.DeltaTime;
		}
		else if (m_ShouldLoop)
		{
			// Pass dirty=true so ParticleFrame resets emitter lifetime and particle count
			m_PlaybackLeft = PLAYBACK_DURATION - InDeltaTimeData.DeltaTime;
			m_ParticleManager->ParticleFrame(InDeltaTimeData.DeltaTime, m_ParticleConfig, true);
		}
	}

	// EndFrame renders particles, ImGui, and presents. Always runs.
	if (m_RendererType == RendererType::Software)
	{
		m_SoftwareRenderer->EndFrame(IsPanelOpen);
	}


	return true;
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

void Application::SetUIandFontScale()
{
    // Initialize ImGui. We are not setting the IniFileName to nullptr because in this case
    // we do want to save the layout state since this is a persistent ui
    IMGUI_CHECKVERSION();
    ImGuiIO& ImGuiIO = ImGui::GetIO();
    ImGuiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable full keyboard inputs(enter, tab, space, etc)

    ImGui::StyleColorsDark();

    /* Bump padding, spacing, scrollbar width, etc. by the same 1.5x factor
     * as the font so widgets don't look cramped around the bigger text.
     * ImGui auto sizes content rects to fit the font, but the gaps between
     * widgets stay at defaults unless we scale them ourselves.
    */
    constexpr float UIScale = 1.5f;
    ImGui::GetStyle().ScaleAllSizes(UIScale);
    /*
     * SDL_GetBasePath() get the directory where the app is RAN FROM
     * This means if we run the executable directly by double-clicking, etc
     * Then it resolves to the path to the binary, and we can just use relative path to
     * find the roboto font(which is much nicer to look at than the default)
     * However, if the app is ran by make run from the root, this would resolve to the project root
     * And we would not be able to find the Roboto font which was copied by CMake to next to the executable
     * So in that case we fall back to the default font
     */
    constexpr float BaseFontSize = 16.f * UIScale;
    const float DpiScale = SDL_GetWindowDisplayScale(m_Window);
    const char* BasePath = SDL_GetBasePath();
    std::string FontPath;
    if (BasePath == nullptr)
    {
        FontPath = "";
    }
    else
    {
        FontPath = BasePath;
    }
    FontPath += "Roboto-Medium.ttf";
    ImFont* LoadedFont = ImGuiIO.Fonts->AddFontFromFileTTF(FontPath.c_str(), BaseFontSize * DpiScale);
    if (LoadedFont == nullptr)
    {
        std::cerr << "Failed to load font from '" << FontPath
                  << "'. Falling back to ImGui default font." << std::endl;
        ImGuiIO.Fonts->AddFontDefault();
    }
    /* On HiDPI (Retina) we rasterize at BaseFontSize * DpiScale, so the
     * font texture has the extra detail, then set FontGlobalScale = 1/DpiScale
     * so ImGui draws the texts at their base 36 units size in the logical
     * canvas. Combined with SDL_SetRenderLogicalPresentation above, the
     * font texture maps 1:1 to physical pixels on a 2x display, pixel-perfect
     * crisp text without blurry upscaling.
     */
    ImGuiIO.FontGlobalScale = 1.f / DpiScale;
}

float Application::GetDeltaTime(uint64_t& LastNs)
{
	float DeltaTime = 0.f;

	// Calculate delta time
	const uint64_t CurrentNs = SDL_GetTicksNS();
	const uint64_t ElapsedNs = CurrentNs - LastNs;
	LastNs = CurrentNs;

	// As suggested by the names, times were in nano seconds, gotta convert to seconds
	DeltaTime = static_cast<float>(ElapsedNs) / Commons::Constants::NS_PER_SECOND;

	// Cap it as we said before so things don't explode when we hanged/slowed down
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
	 * Correction - pausing does not skip the entire frame, just the particle renderer's frame()
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
			// We can just get rid of the Running bool and break here alternatively
			continue;
		}
		// Toggle pause
		if (Result.Event == InputEvent::TogglePause)
		{
			m_Paused = !m_Paused;
		}
		Frame(DTData, Result);
	}
}

