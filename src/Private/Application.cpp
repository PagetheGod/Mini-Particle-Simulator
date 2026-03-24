//
// Created by YWvin on 2026/3/23.
//

//Dependencies and STL
#include <iostream>
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

//External libs and STL
#include <format>
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"

//Own headers
#include "Application.hpp"
#include "Commons.hpp"
Application::Application() : m_Window(nullptr), m_RendererType(RendererType::Software), m_SoftwareRenderer(nullptr)
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
    return true;
}

bool Application::Frame()
{
    //SDL Version of App main event loop
	bool Running = true;
	bool Paused = false;
	bool IsLMBPressed = false;
	uint64_t LastNs = SDL_GetTicksNS();
	float DeltaTime = 0.f;

	//FPS Counter
	int FrameCount = 0;
	float FPSTimer = 0.f;


	while (Running)
	{

		DeltaTime = GetDeltaTime(LastNs);

		//Fps counter
		FrameCount++;
		FPSTimer += DeltaTime;

		if (FPSTimer >= 1.0)
		{
			std::string FPSTitle = std::format("FPS: {} (dt: {:.2f}ms)", FrameCount, DeltaTime * 1000.0);
			SDL_SetWindowTitle(m_Window, FPSTitle.data());
			FrameCount = 0;
			FPSTimer -= 1.0;
		}

		SDL_Event Event;

		//Poll event does not block. It just checks message queue and returns right away regardless of presence of events
		while (SDL_PollEvent(&Event))
		{
			switch (Event.type)
			{
				case SDL_EVENT_QUIT:
				{
					Running = false;
					break;
				}
				//Event.key will contain a kb data
				//key.key is the key code
				//key.mod is the modifier state(Shift, Alt, Ctrl)
				//key.repeat is this is a key-repeat event
				case SDL_EVENT_KEY_DOWN:
				{
					//For now we ignore repeats, which is sent when user hold down the keys
					if (Event.key.repeat)
					{
						break;
					}
					switch (Event.key.key)
					{
					case SDLK_ESCAPE:
					{
						Running = false;
						break;
					}
					case SDLK_SPACE:
					{
						Paused = !Paused;
						std::cout << "Pause status: " << Paused << '\n';
						break;
					}
					case SDLK_F11:
					{
						//Toggle fullscreen, SDL_GetWindowFlags() return a bunch of flags, bitwise and to get the fullscreen flag
						uint64_t Flags = SDL_GetWindowFlags(Window);
						bool IsFullScreen = Flags & SDL_WINDOW_FULLSCREEN;
						SDL_SetWindowFullscreen(Window, !IsFullScreen);
						std::cout << "Fullscreen status: " << IsFullScreen << '\n';
						break;
					}
					default:
						break;
					}
					break;
				}
				//Event.button contains mouse data
				//button.button contains which button, 1 = left, 2 = mid, 3 = right
				//button.x/y is the click position
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				{
					if (Event.button.button == SDL_BUTTON_LEFT)
					{
						IsLMBPressed = true;
						std::cout << "Left click at: (" << Event.button.x << ", " << Event.button.y << ")" << '\n';
					}
					else if (Event.button.button == SDL_BUTTON_RIGHT)
					{
						std::cout << "Right click at: (" << Event.button.x << ", " << Event.button.y << ")" << '\n';
					}
					break;
				}
				case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					if (Event.button.button == SDL_BUTTON_LEFT)
					{
						IsLMBPressed = false;
					}
					break;
				}
				case SDL_EVENT_MOUSE_WHEEL:
				{
					// Scroll wheel. event.wheel.y is the scroll amount.
					// Positive = scroll up, negative = scroll down.
					// Useful for zooming the camera.
					if (Event.wheel.y != 0.0f)
					{
						std::printf("Scroll: %.1f\n", Event.wheel.y);
					}
					break;
				}


				//Mouse movement. Its event data stores both current pos and delta since LAST EVENT
				case SDL_EVENT_MOUSE_MOTION:
				{
					if (IsLMBPressed)
					{
						std::cout << "Dragged activated!" << '\n';
					}
					break;
				}
				//Event.window has the window data, 1 for width and 2 for height
				case SDL_EVENT_WINDOW_RESIZED:
				{
					int NewWidth = Event.window.data1;
					int NewHeight = Event.window.data2;
					std::cout << "Window Resized, width and height are: " << NewWidth << " and " << NewHeight << '\n';
					//Note if our window is resized we HAVE TO recreate vulkan swap chain
					break;
				}
				case SDL_EVENT_WINDOW_MINIMIZED:
				{
					//Stop rendering when we are minimized
					std::cout << "Window Minimized!" << '\n';
					break;
				}
				case SDL_EVENT_WINDOW_RESTORED:
				{
					//Window got restored from a minimized state
					std::cout << "Window Restored!" << '\n';
					break;
				}
			}
		}
		uint64_t FrameEndNs = SDL_GetTicksNS();
		uint64_t FrameDurationNs = FrameEndNs - CurrentNs;
		Uint64 TargetNs = 16666667;  // 1/60 second in nanoseconds
		if (FrameDurationNs < TargetNs)
		{
			SDL_DelayPrecise(TargetNs - FrameDurationNs);
		}
	}
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
