#include "Commons.hpp"
#include "TestHarness.hpp"

TEST_CASE(Commons_LayoutOpenViewportMatchesConstants)
{
    // When the panel is open, GetViewportRect(true) should agree with the
    // canonical layout constants used throughout the UI code.
    const auto viewport = Commons::Layout::GetViewportRect(true);

    // The open viewport starts at the top-left corner of the render area.
    REQUIRE_NEAR(viewport.X, 0.0f, 0.001f);
    REQUIRE_NEAR(viewport.Y, 0.0f, 0.001f);
    // Width and height should match the documented open-panel dimensions.
    REQUIRE_NEAR(viewport.Width, Commons::Layout::VIEWPORT_WIDTH_OPEN, 0.001f);
    REQUIRE_NEAR(viewport.Height, Commons::Layout::VIEWPORT_HEIGHT_OPEN, 0.001f);
}

TEST_CASE(Commons_LayoutCollapsedViewportMatchesWindow)
{
    // When the control panel is collapsed, the viewport should expand to the
    // collapsed-window dimensions instead of the open-panel ones.
    const auto viewport = Commons::Layout::GetViewportRect(false);

    REQUIRE_NEAR(viewport.Width, Commons::Layout::VIEWPORT_COLLAPSED_WIDTH, 0.001f);
    REQUIRE_NEAR(viewport.Height, Commons::Layout::VIEWPORT_COLLAPSED_HEIGHT, 0.001f);
}

TEST_CASE(Commons_RandomFloatStaysWithinRange)
{
    // Sample repeatedly so the test covers more than a single lucky draw.
    for (int i = 0; i < 1000; ++i)
    {
        // RandomFloat(min, max) is expected to stay inside [min, max).
        const float sample = Commons::Utility::RandomFloat(-2.5f, 4.5f);
        REQUIRE(sample >= -2.5f);
        REQUIRE(sample < 4.5f);
    }
}

TEST_CASE(Commons_RandomFloat01StaysWithinUnitInterval)
{
    // RandomFloat_01() is the utility used by several spawn helpers, so we
    // check the full unit interval contract over many samples.
    for (int i = 0; i < 1000; ++i)
    {
        const float sample = Commons::Utility::RandomFloat_01();
        REQUIRE(sample >= 0.0f);
        REQUIRE(sample < 1.0f);
    }
}
