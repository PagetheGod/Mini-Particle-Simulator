// External libs and STLs
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "SDL3/SDL_events.h"
// Own headers
#include "InputManager.hpp"
#include "Commons.hpp"

InputResult InputManager::ProcessInput(const bool IsPanelOpen, int WindowWidth, int WindowHeight)
{
	InputResult Result{};
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
			// Event.key will contain a kb data
			// key.key is the key code
			// key.mod is the modifier state(Shift, Alt, Ctrl)
			// key.repeat is this is a key repeat event
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
				// If ImGui wants the mouse (cursor is over a widget), don't
				// let the click start a camera drag underneath it
				if (ImGui::GetIO().WantCaptureMouse)
				{
					Result.Event = InputEvent::NoOp;
					break;
				}
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
				// Don't let scroll over a slider/combo also zoom the camera
				if (ImGui::GetIO().WantCaptureMouse)
				{
					Result.Event = InputEvent::NoOp;
					break;
				}
				// Scroll wheel. event.wheel.y is the scroll amount.
				// Positive = scroll up, negative = scroll down.
				// Useful for zooming the camera.
				glm::vec2 CursorPosition = glm::vec2(0.f);
				SDL_GetMouseState(&CursorPosition.x, &CursorPosition.y);
				if (!IsCursorInViewport(CursorPosition, IsPanelOpen, WindowWidth, WindowHeight))
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
				// Drop motion events when ImGui owns the mouse so dragging a
				// slider doesn't also pan the camera
				if (ImGui::GetIO().WantCaptureMouse)
				{
					Result.Event = InputEvent::NoOp;
					break;
				}
				if (m_IsLMBPressed)
				{
					if (!IsCursorInViewport(glm::vec2(Event.motion.x, Event.motion.y), IsPanelOpen, WindowWidth, WindowHeight))
					{
						Result.Event = InputEvent::NoOp;
						break;
					}
					// Note that SDL3's Y positive goes DOWN, so we negate it here
					Result.MouseDelta += glm::vec2(Event.motion.xrel, -Event.motion.yrel);
					// Tag this as a camera-drag event so Application::Frame routes it
					// to Camera2D::Pan / Camera::Orbit. Without this the delta is
					// accumulated but never consumed.
					Result.Event = InputEvent::CameraPan;
				}
				else
				{
					Result.Event = InputEvent::NoOp;
				}
				break;
			}
		    case SDL_EVENT_WINDOW_RESIZED:
			{
				// Window resized, it had been resized to data1 x data2, inside the window event struct
				Result.WindowResized = true;
				break;
			}
			default:
				break;
		}
	}
	return Result;
}

bool InputManager::IsCursorInViewport(const glm::vec2& CursorPosition, const bool IsPanelOpen, int WindowWidth, int WindowHeight)
{
	using namespace Commons;
	// Live window size so the hit-test tracks the viewport after a resize (was the fixed overload).
	const Layout::ViewportRect Viewport = Layout::GetViewportRect(IsPanelOpen, WindowWidth, WindowHeight);
	return CursorPosition.x >= Viewport.X && CursorPosition.x <= Viewport.X + Viewport.Width &&
		CursorPosition.y >= Viewport.Y && CursorPosition.y <= Viewport.Y + Viewport.Height;
}
