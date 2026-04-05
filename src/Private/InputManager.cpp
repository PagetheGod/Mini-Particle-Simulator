// External libs and STLs
#include <iostream>
#include "imgui_impl_sdl3.h"
#include <format>
#include "SDL3/SDL_render.h"
// Own headers
#include "InputManager.hpp"
#include "Commons.hpp"

InputResult InputManager::ProcessInput(const bool IsPanelOpen)
{
	InputResult Result = InputResult{};
	SDL_Event Event;
	//Poll event does not block. It just checks message queue and returns right away regardless of presence of events
	while (SDL_PollEvent(&Event))
	{
		ImGui_ImplSDL3_ProcessEvent(&Event);
		switch (Event.type)
		{
			case SDL_EVENT_QUIT:
			{
				Result.Event = InputEvent::Quit;
				return Result;
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
						Result.Event = InputEvent::Quit;
						return Result;
					}
					case SDLK_SPACE:
					{
						Result.Event = InputEvent::TogglePause;
						break;
					}
					case SDLK_TAB:
					{
						Result.Event = InputEvent::ToggleViewport;
						break;
					}
					/*
					case SDLK_F11:
					{
						//Toggle fullscreen, SDL_GetWindowFlags() return a bunch of flags, bitwise and to get the fullscreen flag
						const uint64_t Flags = SDL_GetWindowFlags(m_Window);
						const bool IsFullScreen = Flags & SDL_WINDOW_FULLSCREEN;
						SDL_SetWindowFullscreen(m_Window, !IsFullScreen);
						break;
					}*/
					default:
						break;
				}
				break;
			}
			// Event.button contains mouse data
			// button.button contains which button, 1 = left, 2 = mid, 3 = right
			// button.x/y is the click position
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			{
				if (Event.button.button == SDL_BUTTON_LEFT)
				{
					m_IsLMBPressed = true;
				}
				Result.Event = InputEvent::NoOp;
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				if (Event.button.button == SDL_BUTTON_LEFT)
				{
					m_IsLMBPressed = false;
				}
				Result.Event = InputEvent::NoOp;
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL:
			{
				// Scroll wheel. event.wheel.y is the scroll amount.
				// Positive = scroll up, negative = scroll down.
				// Useful for zooming the camera.
				glm::vec2 CursorPosition = glm::vec2(0.f);
				SDL_GetMouseState(&CursorPosition.x, &CursorPosition.y);
				if (!IsCursorInViewport(CursorPosition, IsPanelOpen))
				{
					Result.Event = InputEvent::NoOp;
					break;
				}
				Result.Event = InputEvent::CameraZoom;
				Result.ScrollDelta += Event.wheel.y; // In case multiple events come at a same time, we accumulate continuous events
				Result.MousePosition = CursorPosition; // For zoom-at-cursor (2D path)
				break;
			}
			// Mouse movement. Its event data stores both current pos and delta since LAST EVENT
			case SDL_EVENT_MOUSE_MOTION:
			{
				if (m_IsLMBPressed)
				{
					if (!IsCursorInViewport(glm::vec2(Event.motion.x, Event.motion.y), IsPanelOpen))
					{
						Result.Event = InputEvent::NoOp;
						break;
					}
					// Note that SDL3's Y positive goes DOWN, so we negate it here
					Result.MouseDelta += glm::vec2(Event.motion.xrel, -Event.motion.yrel);
				}
				else
				{
					Result.Event = InputEvent::NoOp;
				}
				break;
			}
			default:
				break;
		}
	}
	return Result;
}

bool InputManager::IsCursorInViewport(const glm::vec2& CursorPosition, const bool IsPanelOpen)
{
	using namespace Commons;
	const Layout::ViewportRect Viewport = Layout::GetViewportRect(IsPanelOpen);
	return CursorPosition.x >= Viewport.X && CursorPosition.x <= Viewport.X + Viewport.Width &&
		CursorPosition.y >= Viewport.Y && CursorPosition.y <= Viewport.Y + Viewport.Height;
}
