#include "Commons.hpp"
#include "TestHarness.hpp"

TEST_CASE(Commons_LayoutOpenViewportMatchesConstants)
{
    const auto viewport = Commons::Layout::GetViewportRect(true);

    REQUIRE_NEAR(viewport.X, 0.0f, 0.001f);
    REQUIRE_NEAR(viewport.Y, 0.0f, 0.001f);
    REQUIRE_NEAR(viewport.Width, Commons::Layout::VIEWPORT_WIDTH_OPEN, 0.001f);
    REQUIRE_NEAR(viewport.Height, Commons::Layout::VIEWPORT_HEIGHT_OPEN, 0.001f);
}

TEST_CASE(Commons_LayoutCollapsedViewportMatchesWindow)
{
    const auto viewport = Commons::Layout::GetViewportRect(false);

    REQUIRE_NEAR(viewport.Width, Commons::Layout::VIEWPORT_COLLAPSED_WIDTH, 0.001f);
    REQUIRE_NEAR(viewport.Height, Commons::Layout::VIEWPORT_COLLAPSED_HEIGHT, 0.001f);
}

TEST_CASE(Commons_RandomFloatStaysWithinRange)
{
    for (int i = 0; i < 1000; ++i)
    {
        const float sample = Commons::Utility::RandomFloat(-2.5f, 4.5f);
        REQUIRE(sample >= -2.5f);
        REQUIRE(sample < 4.5f);
    }
}

TEST_CASE(Commons_RandomFloat01StaysWithinUnitInterval)
{
    for (int i = 0; i < 1000; ++i)
    {
        const float sample = Commons::Utility::RandomFloat_01();
        REQUIRE(sample >= 0.0f);
        REQUIRE(sample < 1.0f);
    }
}
