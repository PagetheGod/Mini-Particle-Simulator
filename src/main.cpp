/*
 * Mini Particle Simulator project, built by Iso and Vincent
 * This is a tiny particle simulator/previewer that aims to create a simplified experience from game engine particle vfx editor
 */
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "Application.hpp"

int main()
{
    Application App;
    if (!App.Initialize())
    {
        std::cerr << "Application failed to initialize!" << std::endl;
        return EXIT_FAILURE;
    }
    App.Run();
    return EXIT_SUCCESS;
}
