# Mini Particle Simulator Makefile
# This is a thin wrapper that just invokes CMake
# Usage:
#  make: Release build (default)
#  make BUILD_TYPE=Debug: Debug build
#  make run: Build and run the app
#  make test: Build and run CTest (both core_tests and threadpool_catch_tests)
#  make catch_test: Build and run only the course-provided Catch2 ThreadPool tests
#  make clean: Delete build artifacts for current config
#  make clobber: Delete the current build dir entirely to clean up all build artifacts

# Finally looked up what this .PHONY thing means(been seeing it in our assignment Makefile)
# I think it tells make "hey these commands are not files" so make do not get confused
# Otherwise if some files in the directory happen to have same names as these commands
# make will run the files instead of these commands
.PHONY: all configure app tests catch_tests font run test catch_test clean clobber

# User-overridable variables
# `?=` only sets a variable if it isn't already defined, so command-line args
# overrides like make BUILD_TYPE=Debug.
# Basically, it looks like a ternary, and behaves like a ternary, lol
# Default to release mode
BUILD_TYPE ?= Release
CMAKE ?= cmake
APP_TARGET ?= MiniParticleSimulator
TEST_TARGET ?= core_tests
CATCH_TEST_TARGET ?= threadpool_catch_tests

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
                         -DBUILD_TESTING=ON \
                         -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Targets
all: app tests catch_tests

configure:
	$(CMAKE) $(CMAKE_CONFIGURE_FLAGS)

app: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(APP_TARGET) -j 4

tests: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(TEST_TARGET) -j 4

# Build the course-provided Catch2 ThreadPool tests.
# Kept as a separate target so a failure in the provided tests doesn't
# block building the app or our own core_tests harness.
catch_tests: configure
	$(CMAKE) --build $(BUILD_DIR) --target $(CATCH_TEST_TARGET) -j 4

font: configure
	$(CMAKE) -E copy_if_different $(FONT_SRC) $(FONT_DST)

run: app font
	./$(EXE_DIR)/$(APP_TARGET)

test: tests catch_tests
	ctest --test-dir $(BUILD_DIR) --output-on-failure

# Build + run only the course-provided Catch2 ThreadPool tests.
# Useful when iterating on ThreadPool.cpp without rebuilding everything.
catch_test: catch_tests
	./$(EXE_DIR)/$(CATCH_TEST_TARGET)

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean

clobber:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
