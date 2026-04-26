# Requires cmake 4.2.3
cmake_minimum_required(VERSION 4.2.3)

# Define project name and its languages
project(
	MiniParticleSimulator
	VERSION 0.1.0
	DESCRIPTION "A mini, simplified particle simulator."
	LANGUAGES C CXX
)

include(CTest)

# C++ stuffs, set required C++ standards and disable compiler-specific extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Verify C++23 support with Clang 17+
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
    message(FATAL_ERROR
        "Clang 17+ is required for C++23 support. "
        "You have Clang ${CMAKE_CXX_COMPILER_VERSION}. "
        "Install a newer version"
    )
endif()





# Output Directories, as it stands, this builds everything into a bin folder
# I would like it to be a less nested but sometimes less is more, CMake is not easy :(
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Tell CMake to build from source via submodule

# SDL3, built from source via submodule
# add_subdirectory() tells CMake: "Go into external/SDL/, find its
# CMakeLists.txt, and build it as part of our project."
# EXCLUDE_FROM_ALL means "don't build SDL3's tests and examples, just
# the library itself."
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
# Add the subdirectory for glm, same deal, exclude from all tells CMake
# to only build parts of glm that we depend on
add_subdirectory(external/glm EXCLUDE_FROM_ALL)
# ---------------------------------------------------------------------------
# Vulkan — find the system installation
# ---------------------------------------------------------------------------
# find_package(Vulkan) searches for the Vulkan SDK installed on your system.
# It uses the VULKAN_SDK environment variable to locate it.
# REQUIRED means CMake will stop with an error if Vulkan is not found.
find_package(Vulkan REQUIRED)




# Our executable
# add_executable() creates a build target exe with the same name as our project from the
# listed source files.
add_executable(${PROJECT_NAME}
    src/main.cpp
        src/Private/Application.cpp
        src/Public/Application.hpp
        src/Private/ParticleManager.cpp
        src/Public/ParticleManager.hpp
        src/Public/Commons.hpp
        src/Private/SoftwareRenderer.cpp
        src/Public/SoftwareRenderer.hpp
        src/Private/UIManager.cpp
        src/Public/UIManager.hpp
        src/Private/HardwareRenderer.cpp
        src/Public/HardwareRenderer.hpp
        src/Private/Camera.cpp
        src/Public/Camera.hpp
        src/Private/InputManager.cpp
        src/Public/InputManager.hpp
        src/Private/ThreadPool.cpp
        src/Public/ThreadPool.hpp
        src/Private/SIMD.cpp
        src/Public/SIMD.hpp
        src/Public/VulkanManager.hpp
        src/Private/VulkanManager.cpp
        src/Private/VThreadPool.cpp
        src/Public/ParticlePresets.hpp
        src/Private/Camera2D.cpp
)

# SIMD.cpp now provides both x86 detection and scalar fallbacks for ARM.
# _xgetbv needs -mxsave on the x86 code path for Clang/GCC.
if(IS_X86_64 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(src/Private/SIMD.cpp PROPERTIES COMPILE_FLAGS "-mxsave")
endif()




# Dear ImGui, compiled directly into the project
# ImGui is designed to be compiled as source files, not as a separate library.
# We include the core files plus TWO backends:
#   - imgui_impl_sdl3: handles input (mouse, keyboard, clipboard) via SDL3
#   - imgui_impl_sdlrenderer3: renders ImGui via SDL_Renderer (software path)
#   - imgui_impl_vulkan: renders ImGui via Vulkan (GPU path)
# Both renderer backends are compiled in. At runtime, we initialize only one
# based on the user's choice at the startup modal.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

target_sources(${PROJECT_NAME} PRIVATE
    # Core ImGui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # Platform backend (always SDL3)
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp

    # Renderer backends (both compiled, one used at runtime)
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp   # Software path
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp         # GPU path
)

# Compiler options: strict warnings and no optimization in Debug, basic
# warnings and full optimization in Release.
# Debug: -Wall -Wextra (strict but not -Wpedantic, which third-party libs
# like ImGui and SDL triggers constantly). Plus -Wno-unused-parameter and
# -Wno-missing-field-initializers because ImGui callbacks and SDL structs
# often trigger those
# Release: -Wall only. Keep the basic set so obvious bugs still show up
# in optimized builds, but drop the others so build warnings don't clutter
# the CLI
target_compile_options(${PROJECT_NAME} PRIVATE
	# MSVC: /W4 is strict, /W1 is the most basic level
    # Is Microsoft normal? Like who the fuck names their warning flags
    # 4100 and 4201, just use human-readable names like GCC and Clang please???
	$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W4 /wd4100 /wd4201 /Od>
	$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/W1 /O2 /Oi /GL>
	# GCC
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -O0>
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Wall -O3 -flto=auto>
	# Clang
	$<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Debug>>:-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -O0>
	$<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Release>>:-Wall -O3 -flto=auto>
)

# Enables link time optimizations for all three compilers. Learned from my ray tracer that this can make a huge difference
# Again, just using clang right now but keeping it here for references
target_link_options(${PROJECT_NAME} PRIVATE
	 $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
 	 $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-flto=auto>
	 $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Release>>:-flto=auto>
)

# Vulkan and HLSL, find the DXC compiler that comes with vulkan
find_program(DXC dxc HINTS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VULKAN_SDK}/bin
)

if(NOT DXC)
    message(FATAL_ERROR
        "Could not find dxc (DirectX Shader Compiler). "
        "Make sure the Vulkan SDK is installed and VULKAN_SDK is set. "
        "Note: The DXC from Visual Studio does NOT support -spirv. "
        "Please use the DXC bundled with the LunarG Vulkan SDK."
    )
endif()

message(STATUS "DXC compiler: ${DXC}")

# Set the shader output path next to the executable
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders)


# Define shader source files with DXC Profile, profile tells compiler what kind of shader are we working with
# This creates a list, where each shader is associated with three items:
# 1. Its own path
# 2. Its shader stage and model(vs/ps/cs, HLSL SM 6.4)
# 3. Its output file name
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.vert.hlsl vs_6_4 Particle.vert
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.frag.hlsl ps_6_4 Particle.frag
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.update.comp.hlsl cs_6_4 Particle_Update.comp
)
# Parse the shader list (triplets: source, profile, output_name)
set(SHADER_BINARIES "") # This creates a list in CMake, it starts out empty
list(LENGTH SHADER_SOURCES SHADER_LIST_LEN) # This line gets the total number of elements in SHADER_SOURCES
math(EXPR SHADER_COUNT "${SHADER_LIST_LEN} / 3") # This evaluates a math expression,
# Start a loop, going from 0 to SHADER_COUNT
# Note that CMake loop is inclusive, so we have one extra iteration
foreach (SHADER_IDX RANGE 0 ${SHADER_COUNT})
    # This prevents us from going into that extra iteration
    if(SHADER_IDX EQUAL SHADER_COUNT)
        break()
    endif()

    math(EXPR SRC_IDX "${SHADER_IDX} * 3")
    math(EXPR PROF_IDX "${SRC_IDX} + 1")
    math(EXPR OUT_IDX "${SRC_IDX} + 2")

    list(GET SHADER_SOURCES ${SRC_IDX} SHADER_SRC)
    list(GET SHADER_SOURCES ${PROF_IDX} SHADER_PROFILE)
    list(GET SHADER_SOURCES ${OUT_IDX} SHADER_OUT_NAME)

    set(SHADER_SPV ${SHADER_OUTPUT_DIR}/${SHADER_OUT_NAME}.spv)

    # DXC compilation command:
    # -spirv: Output SPIR-V (instead of DXIL for DirectX)
    # -T <profile>: Shader stage and model (for example, 6_4)
    # -E main: Entry point function name
    # -Fo <output>: Output file path
    # -fspv-target-env=vulkan1.4: Target Vulkan 1.4 SPIR-V environment
    #  -Zpc: Pack matrices in column-major order (Vulkan convention)
    # -WX: Treat warnings as errors
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${DXC}
            -spirv
            -T ${SHADER_PROFILE}
            -E main
            ${SHADER_SRC}
            -Fo ${SHADER_SPV}
            -fspv-target-env=vulkan1.4
            -Zpc
            -WX
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling HLSL: ${SHADER_OUT_NAME}.hlsl [${SHADER_PROFILE}] → ${SHADER_OUT_NAME}.spv"
        VERBATIM
    )

    list(APPEND SHADER_BINARIES ${SHADER_SPV}) # Add the compiled shader's path to the SHADER_BINARIES list
endforeach()

# Create command that depends on all compiled shaders
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_BINARIES})

# Make our executable depend on the shaders
add_dependencies(${PROJECT_NAME} CompileShaders)



# ---------------------------------------------------------------------------
# Include directories
# target_include_directories() tells the compiler where to find header files
# for our target. This way we do not have to say Public/<Header Path>, instead we
# can just say #include <Header Path>, nice and simple
# And if the IDE supports CMake this also allows the intellisense to work normally
# with those includes.
# PRIVATE means these paths only apply to the executable target
# we added above not to anything that might link against particle_sim.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/src/Public
    ${CMAKE_SOURCE_DIR}/src/ThirdParty/pcg/include
)


# Link libraries
# target_link_libraries() tells the linker which libraries our executable
# needs. SDL3::SDL3 is a CMake imported target that SDL3's build system
# creates. It automatically handles include paths, library paths, and
# linker flags. Vulkan::Vulkan does the same for the Vulkan SDK.
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    glm::glm
)

if(BUILD_TESTING)
    add_executable(core_tests
        tests/test_main.cpp
        tests/Camera2DTests.cpp
        tests/CommonsTests.cpp
        tests/ParticleManagerTests.cpp
        src/Private/ParticleManager.cpp
        src/Private/VThreadPool.cpp
        src/Private/SIMD.cpp
        src/Private/Camera2D.cpp
    )

    if(IS_X86_64 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set_source_files_properties(src/Private/SIMD.cpp PROPERTIES COMPILE_FLAGS "-mxsave")
    endif()

    target_include_directories(core_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Public
        ${CMAKE_SOURCE_DIR}/external/glm
        ${CMAKE_SOURCE_DIR}/src/ThirdParty/pcg/include
    )

    if(MSVC)
        target_compile_options(core_tests PRIVATE /W4)
    else()
        target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME core_tests COMMAND core_tests)

    # Build the course provided catch2 based tests for thread pool
    # We have to build this as a separate executable
    # Because it has its own main and would conflict with our own test main
    # if we got multiple entry points
    add_executable(threadpool_catch_tests
        tests/catch.cpp
        tests/test_suite.cpp
        tests/test_threadpool.cpp
        src/Private/ThreadPool.cpp
    )

    target_include_directories(threadpool_catch_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/Public
        ${CMAKE_SOURCE_DIR}/tests
    )

    if(MSVC)
        target_compile_options(threadpool_catch_tests PRIVATE /W4)
    else()
        # Catch2 sources will trigger many pedantic warnings. Relax warnings for
        # this target only so third party and provided code don't fail builds.
        target_compile_options(threadpool_catch_tests PRIVATE -w)
    endif()

    add_test(NAME threadpool_catch_tests COMMAND threadpool_catch_tests)
endif()


# Windows: Copy SDL3.dll next to the executable
# On Windows, DLLs must be in the same directory as the .exe (or on PATH).
# This post-build command copies SDL3.dll into our output directory
# automatically after every build. On Linux, shared libraries are found
# via the system's library search path, so this isn't needed.
# Also, VERBATIM basically means that these commands get escaped properly to the
# build tools so they can be invoked unchanged, CMake official documentations
# recommend adding this to add_custom_command
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL3.dll to output directory"
        VERBATIM
    )
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/external/imgui/misc/fonts/Roboto-Medium.ttf
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying Roboto-Medium.ttf to output directory"
        VERBATIM
    )
endif()
