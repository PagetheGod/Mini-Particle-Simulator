# Mini Particle Simulator Makefile
# This is a thin wrapper that just invokes CMake
# Usage:
#  make: Release build (default)
#  make BUILD_TYPE=Debug: Debug build
#  make run: Build and run the app
#  make clean: Delete build artifacts for current config
#  make clobber: Delete the current build dir entirely to clean up all build artifacts

# Finally looked up what this .PHONY thing means(been seeing it in our assignment Makefile)
# I think it tells make "hey these commands are not files" so make do not get confused
# Otherwise if some files in the directory happen to have same names as these commands
# make will run the files instead of these commands
.PHONY: all configure app font run clean clobber

# User-overridable variables
# `?=` only sets a variable if it isn't already defined, so command-line args
# overrides like make BUILD_TYPE=Debug.
# Basically, it looks like a ternary, and behaves like a ternary, lol
# Default to release mode
BUILD_TYPE ?= Release
CMAKE ?= cmake
APP_TARGET ?= MiniParticleSimulator

# One build dir per build type so switching between Debug and Release doesn't
# invalidate the other one's CMake cache.
# For now the project only supports Clang, maybe add MSVC and gcc in the future
BUILD_DIR ?= build/$(BUILD_TYPE)
EXE_DIR := $(BUILD_DIR)/bin
FONT_SRC := external/imgui/misc/fonts/Roboto-Medium.ttf
FONT_DST := $(EXE_DIR)/Roboto-Medium.ttf

# CMake flags
CMAKE_CONFIGURE_FLAGS := -S . -B $(BUILD_DIR) \
                         -DCMAKE_C_COMPILER=clang \
                         -DCMAKE_CXX_COMPILER=clang++ \
                         -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
                         -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Targets
all: app

configure:
	$(CMAKE) $(CMAKE_CONFIGURE_FLAGS)

app: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(APP_TARGET) -j 4

font: configure
	$(CMAKE) -E copy_if_different $(FONT_SRC) $(FONT_DST)

run: app font
	./$(EXE_DIR)/$(APP_TARGET)

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean

clobber:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
