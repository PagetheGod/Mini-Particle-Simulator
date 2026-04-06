#pragma once

#include <string>
#include <cstdint>
#include <random>

// The PCG random lib I grabbed has virtually no MSVC support
// So guarding against that
#ifndef _MSC_VER
    #include "pcg_random.hpp"
#endif


namespace Commons
{
    namespace Utility
    {
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
            thread_local std::mt19937 RandomGenerator(RandomDevice);
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
         * ---------------------------------------------------------------------------
         * We cap delta time to prevent the "spiral of death" — if the app freezes
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

    }
    namespace Layout
    {
        // The size of the entire GUI window, this is fixed at start-up
        // Not supporting resizing at the moment because it's too much work
        static constexpr int WINDOW_WIDTH = 1920;
        static constexpr int WINDOW_HEIGHT = 1080;

        // Supersampling render resolution
        // Particles render at this resolution in the offscreen target,
        // then get downscaled to the viewport region via blit.
        // 2560×1440 at a 1920×1080 window = ~1.78x supersampling.
        static constexpr int RENDER_WIDTH = 2560;
        static constexpr int RENDER_HEIGHT = 1440;

        // Percentage of the window occupied by render viewport(horizontal)
        static constexpr float VIEWPORT_PORTION_WIDTH = 0.6f;
        // Percentage of the window occupied by  render viewport(vertical)
        static constexpr float VIEWPORT_PORTION_HEIGHT = 0.8f;
        // Viewport dimensions when the panels are open
        static constexpr float VIEWPORT_WIDTH_OPEN = WINDOW_WIDTH * VIEWPORT_PORTION_WIDTH;
        static constexpr float VIEWPORT_HEIGHT_OPEN = WINDOW_HEIGHT * VIEWPORT_PORTION_HEIGHT;
        // Panel dimensions
        static constexpr float PANEL_WIDTH = WINDOW_WIDTH - VIEWPORT_WIDTH_OPEN;
        static constexpr float STATUS_BAR_HEIGHT = WINDOW_HEIGHT - VIEWPORT_HEIGHT_OPEN;
        // Derived: viewport sizes when panels are COLLAPSED
        static constexpr float VIEWPORT_COLLAPSED_WIDTH = static_cast<float>(WINDOW_WIDTH);  // 1920
        static constexpr float VIEWPORT_COLLAPSED_HEIGHT = static_cast<float>(WINDOW_HEIGHT); // 1080

        // Helper struct storing viewport dimensions as a "rectangle"
        // Stuffs I learned from WIN32 with UserRect, ClientRect
        struct ViewportRect {
            // Starting the viewport at the top-left corner
            // Default to dimensions when the panels are collapsed
            float X = 0.f;
            float Y = 0.f;
            float Width = VIEWPORT_WIDTH_OPEN;
            float Height = VIEWPORT_HEIGHT_OPEN;
        };
        // Helper function to get viewport dimensions based on setting panel state(collapsed/open)
        static ViewportRect GetViewportRect(const bool IsPanelOpen)
        {
            if (IsPanelOpen) {
                return ViewportRect { 0.f, 0.f, VIEWPORT_WIDTH_OPEN, VIEWPORT_HEIGHT_OPEN };
            }
            return ViewportRect { 0.f, 0.f, VIEWPORT_COLLAPSED_WIDTH, VIEWPORT_COLLAPSED_HEIGHT };
        }

    }

}
