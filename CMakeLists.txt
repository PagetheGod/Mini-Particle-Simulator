#Requires cmake 4.2.3
cmake_minimum_required(VERSION 4.2.3)

#Define project name and its languages
project(
	MiniParticleSimulator
	VERSION 0.1.0
	DESCRIPTION "A mini, simplified particle simulator."
	LANGUAGES C CXX
)

#C++ stuffs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Verify C++23 support with Clang 17+
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "17.0")
    message(FATAL_ERROR
        "Clang 17+ is required for C++23 support. "
        "You have Clang ${CMAKE_CXX_COMPILER_VERSION}. "
        "Install a newer version or use your course's Linux environment."
    )
endif()





#Output Directories, as it stands, this builds everything into a bin folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

#Tell CMake to build from source via submodule
# ---------------------------------------------------------------------------
# SDL3 — built from source via submodule
# ---------------------------------------------------------------------------
# add_subdirectory() tells CMake: "Go into external/SDL/, find its
# CMakeLists.txt, and build it as part of our project."
# EXCLUDE_FROM_ALL means "don't build SDL3's tests and examples — just
# the library itself."
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Vulkan — find the system installation
# ---------------------------------------------------------------------------
# find_package(Vulkan) searches for the Vulkan SDK installed on your system.
# It uses the VULKAN_SDK environment variable to locate it.
# REQUIRED means CMake will stop with an error if Vulkan is not found.
find_package(Vulkan REQUIRED)




# ---------------------------------------------------------------------------
# Our executable
# ---------------------------------------------------------------------------
# add_executable() creates a build target called "particle_sim" from the
# listed source files. We'll add more .cpp files here as the project grows.
add_executable(${PROJECT_NAME}
    src/main.cpp
        src/Public/Application.cpp
        src/Public/Application.hpp
)


 # ---------------------------------------------------------------------------
# Compiler warnings — strict but not pedantic
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused_paramter # ImGui callbacks often have unused params
        -Wno-missing-field-initializers  # Vulkan structs use partial init
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4100 /wd4201)
endif()



 # ---------------------------------------------------------------------------
# Dear ImGui — compiled directly into the project
# ---------------------------------------------------------------------------
# ImGui is designed to be compiled as source files, not as a separate library.
# We include the core files plus TWO backends:
#   - imgui_impl_sdl3: handles input (mouse, keyboard, clipboard) via SDL3
#   - imgui_impl_sdlrenderer3: renders ImGui via SDL_Renderer (software path)
#   - imgui_impl_vulkan: renders ImGui via Vulkan (GPU path)
#
# Both renderer backends are compiled in. At runtime, we initialize only one
# based on the user's choice at the startup modal.
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

target_sources(${PROJECT_NAME} PRIVATE
    # --- Core ImGui ---
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp       # Optional but useful for learning
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    # --- Platform backend (always SDL3) ---
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp

    # --- Renderer backends (both compiled, one used at runtime) ---
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp   # Software path
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp         # GPU path
)

# Allows us to usue these paths directly without having to specify
target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_SOURCE_DIR}/src/Public
)



#Set up debug and release compiler options for all three compilers
target_compile_options(${PROJECT_NAME} PRIVATE
	#MSVC
	$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W4 /Od>
	$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/W1 /O2 /Oi /GL>
	#GCC
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-Wall -Wextra -O0>
	$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-w -O3 -flto=auto>
	#Clang
	$<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Debug>>:-Wall -Wextra -O0>
	$<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Release>>:-w -O3 -flto=auto>
)

#Enables link time optimizations for all three compilers. Learned from my ray tracer that this can make a huge difference

target_link_options(${PROJECT_NAME} PRIVATE
	 $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
 	 $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-flto=auto>
	 $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Release>>:-flto=auto>
)

#Vulkan and HLSL, find the DXC compiler that comes with vulkan 
find_program(DXC dxc HINTS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VULKAN_SDK}/bin
)

if(NOT DXC)
    message(FATAL_ERROR
        "Could not find dxc (DirectX Shader Compiler). "
        "Make sure the Vulkan SDK is installed and VULKAN_SDK is set. "
        "Note: The DXC from Visual Studio does NOT support -spirv. "
        "You need the one bundled with the LunarG Vulkan SDK."
    )
endif()

message(STATUS "DXC compiler: ${DXC}")

#Set the shader output path next to the executable
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders)

#Define shader source files with DXC Profile, profile tells compiler what kind of shader are we working with
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.vert.hlsl vs_6_0 Particle.vert
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.frag.hlsl ps_6_0 Particle.frag
    ${CMAKE_SOURCE_DIR}/src/Shaders/Particle.update.comp.hlsl cs_6_0 Particle_Update.comp
)
# Parse the shader list (triplets: source, profile, output_name)
set(SHADER_BINARIES "")
list(LENGTH SHADER_SOURCES SHADER_LIST_LEN)
math(EXPR SHADER_COUNT "${SHADER_LIST_LEN} / 3")

foreach (SHADER_IDX RANGE 0 ${SHADER_COUNT})
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
    #   -spirv        : Output SPIR-V (instead of DXIL for DirectX)
    #   -T <profile>  : Shader stage and model (e.g., vs_6_0)
    #   -E main       : Entry point function name
    #   -Fo <output>  : Output file path
    #   -fspv-target-env=vulkan1.4 : Target Vulkan 1.4 SPIR-V environment
    #   -Zpc          : Pack matrices in column-major order (Vulkan convention)
    #   -WX           : Treat warnings as errors
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

    list(APPEND SHADER_BINARIES ${SHADER_SPV})
endforeach()

#Create command thast depends on all compiled shaders
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_BINARIES})

#Make our executable depend on the shaders
add_dependencies(${PROJECT_NAME} CompileShaders)


# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
# target_include_directories() tells the compiler where to find header files
# for our target. "PRIVATE" means these paths only apply to particle_sim,
# not to anything that might link against particle_sim.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
# target_link_libraries() tells the linker which libraries our executable
# needs. SDL3::SDL3 is a CMake "imported target" that SDL3's build system
# creates. It automatically handles include paths, library paths, and
# linker flags. Vulkan::Vulkan does the same for the Vulkan SDK.
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
)

# ---------------------------------------------------------------------------
# Windows: Copy SDL3.dll next to the executable
# ---------------------------------------------------------------------------
# On Windows, DLLs must be in the same directory as the .exe (or on PATH).
# This post-build command copies SDL3.dll into our output directory
# automatically after every build. On Linux, shared libraries are found
# via the system's library search path, so this isn't needed.
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL3.dll to output directory"
        VERBATIM
    )
endif()