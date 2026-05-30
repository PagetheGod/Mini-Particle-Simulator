#pragma once
#include <random>
#include <cstdarg>
#include <cstdio>
#include <SDL3/SDL_messagebox.h>
// The PCG random lib I grabbed has virtually no MSVC support
// So guarding against that
#ifndef _MSC_VER
    #include "pcg_random.hpp"
#endif


namespace Commons
{
    namespace Utility
    {
        /*
         * Surfaces an error to the user via a modal SDL message box.
         * Use this for failures that would otherwise be invisible: on Windows GUI
         * apps stderr usually isn't attached to anything, and the window can close
         * before the user reads anything we print. The message box blocks until OK
         * is pressed, so the caller can still return false right after and have
         * the user actually see what went wrong.
         * Format follows printf rules.
         */
        static inline void ShowError(const char* Title, const char* Format, ...)
        {
            char Buffer[512];
            va_list Args;
            va_start(Args, Format);
            vsnprintf(Buffer, sizeof(Buffer), Format, Args);
            va_end(Args);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Title, Buffer, nullptr);
        }

        // Helper to generate a random floating point number between [Min, Max)
        static inline float RandomFloat(const float Min, const float Max)
        {
        #ifndef _MSC_VER
            // Seed with a real random value if possible
            thread_local pcg_extras::seed_seq_from<std::random_device> SeedSource;
            // Make a random number engine
            thread_local pcg32 RandomGenerator(SeedSource);
            // Generate a random float between min and max
            std::uniform_real_distribution<float> Distribution(Min, Max);
            return Distribution(RandomGenerator);
            // Following logics are for MSVC because PCG does not support MSVC at all
            // Come on man
        #else
            thread_local std::random_device RandomDevice;
            std::uniform_real_distribution<float> Distribution(Min, Max);
            thread_local std::mt19937 RandomGenerator(RandomDevice());
            return Distribution(RandomGenerator);
        #endif
        }

        // Use the above helper to get a float between 0 and 1
        static inline float RandomFloat_01()
        {
            return RandomFloat(0.f, 1.f);
        }
    }
    namespace Constants
    {
        static constexpr const char* WINDOW_TITLE = "Mini Particle Simulator";

        /*
         * Timing constants
         * We cap delta time to prevent the "spiral of death", if the app freezes
         * for 2 seconds (e.g., breakpoint in debugger, OS stall), the next frame
         * would get dt=2.0. Physics would explode because particles would try to
         * move 2 seconds' worth of distance in one step, potentially tunneling
         * through colliders or flying off to infinity.
         * By capping dt, we accept that the simulation will run in slow-motion
         * during stalls rather than producing garbage.
         */
        static constexpr double MAX_DELTA_TIME = 1.0 / 15.0;  // ~66.7ms cap
        // Nanoseconds per second — for converting SDL_GetTicksNS() to seconds.
        static constexpr double NS_PER_SECOND = 1'000'000'000.0;
        // Max number of forces we can have other than gravity
        static constexpr uint8_t MAX_NUM_FORCES = 9;
        // Gravity!!!
        static constexpr float GRAVITY = 9.81f;
        // Custom epsilon since the one used by GLM is really small
        static constexpr float CUSTOM_EPSILON = 0.00001f;
        // Worker thread count requested for the shared thread pool. The pool
        // clamps this down to (hardware_concurrency - 1) internally, so asking
        // for more than the machine has is safe.
        static constexpr uint32_t NUM_THREADS_USED = 16;

    }
    namespace Layout
    {
        // Supersampling render resolution
        // Particles render at this resolution in the offscreen target,
        // then get downscaled to the viewport region via blit.
        // 2560×1440 at a 1920×1080 window = ~1.78x supersampling.
        static constexpr int RENDER_WIDTH = 2560;
        static constexpr int RENDER_HEIGHT = 1440;
        // Window width and height, this is just for the default start up window
        // Since the app is now resizable
        static constexpr int START_WINDOW_WIDTH = 1980;
        static constexpr int START_WINDOW_HEIGHT = 1080;
        // Aspect ratio
        static constexpr float ASPECT_RATIO = 16.f / 9.f;
        // Percentage of the window occupied by render viewport(horizontal)
        static constexpr float VIEWPORT_PORTION_WIDTH = 0.6f;
        // Percentage of the window occupied by  render viewport(vertical)
        static constexpr float VIEWPORT_PORTION_HEIGHT = 0.8f;

        // Helper struct storing viewport dimensions as a "rectangle"
        // Stuffs I learned from WIN32 with UserRect, ClientRect
        struct ViewportRect {
            // Starting the viewport at the top-left corner
            // Default to dimensions when the panels are collapsed
            float X = 0.f;
            float Y = 0.f;
            float Width = 1920.f;
            float Height = 1080.f;
        };
        // Helper function to get viewport dimensions based on setting panel state(collapsed/open)
        // Since we are now supporting resizing, this do a little bit more, it now calculates the actual sizes in real time
        inline ViewportRect GetViewportRect(const bool IsPanelOpen)
        {
            // This overload is for the software path only, where SDL3 stretches things to fit
            if (IsPanelOpen) {
                return ViewportRect { 0.f, 0.f, RENDER_WIDTH * VIEWPORT_PORTION_WIDTH,
                    RENDER_HEIGHT * VIEWPORT_PORTION_HEIGHT };
            }
            return ViewportRect { 0.f, 0.f, RENDER_WIDTH, RENDER_HEIGHT };
        }
        inline ViewportRect GetViewportRect(const bool IsPanelOpen, int WindowWidth, int WindowHeight)
        {
            // If the settings panel is opened, the viewport only occupies a portion of the screen
            if (IsPanelOpen)
            {
                return ViewportRect { 0.f, 0.f, static_cast<float>(WindowWidth) * VIEWPORT_PORTION_WIDTH,
                    static_cast<float>(WindowHeight) * VIEWPORT_PORTION_HEIGHT };
            }
            return ViewportRect { 0.f, 0.f, static_cast<float>(WindowWidth), static_cast<float>(WindowHeight) };
        }

    }

}
