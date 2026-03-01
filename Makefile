# Root Makefile for Coral Cubed Flight Project
# This file configures the build environment and provides targets for the entire project

# ARM Toolchain Configuration
ARM_TOOLCHAIN_DIR := $(CURDIR)/make/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi
ARM_TOOLCHAIN_BIN := $(ARM_TOOLCHAIN_DIR)/bin

# Set up toolchain prefix
export ARM_TOOLCHAIN_PREFIX := arm-none-eabi-

# Add toolchain to PATH so all subsystems can use it
export PATH := $(ARM_TOOLCHAIN_BIN):$(PATH)

# Toolchain tools
export CC := $(ARM_TOOLCHAIN_PREFIX)gcc
export CXX := $(ARM_TOOLCHAIN_PREFIX)g++
export LD := $(ARM_TOOLCHAIN_PREFIX)ld
export AR := $(ARM_TOOLCHAIN_PREFIX)ar
export OBJCOPY := $(ARM_TOOLCHAIN_PREFIX)objcopy
export OBJDUMP := $(ARM_TOOLCHAIN_PREFIX)objdump
export SIZE := $(ARM_TOOLCHAIN_PREFIX)size
export GDB := $(ARM_TOOLCHAIN_PREFIX)gdb
export STRIP := $(ARM_TOOLCHAIN_PREFIX)strip
export NM := $(ARM_TOOLCHAIN_PREFIX)nm
export RANLIB := $(ARM_TOOLCHAIN_PREFIX)ranlib

# Project directories
REPO_ROOT := $(CURDIR)
SRC_DIR := $(REPO_ROOT)/src
THIRD_PARTY_DIR := $(REPO_ROOT)/third-party
LIBOPENCM3_DIR := $(THIRD_PARTY_DIR)/libopencm3
CORAL_MICRO_DIR := $(THIRD_PARTY_DIR)/coralmicro

# Export key directories for use in sub-Makefiles
export REPO_ROOT
export SRC_DIR
export THIRD_PARTY_DIR
export LIBOPENCM3_DIR
export CORAL_MICRO_DIR

# Build directories
BUILD_DIR := build
DIST_DIR := dist

# Determine if toolchain is properly installed
TOOLCHAIN_CHECK := $(shell [ -f "$(ARM_TOOLCHAIN_BIN)/$(ARM_TOOLCHAIN_PREFIX)gcc" ] && echo "found" || echo "")

# Verbose flag (make V=1 for verbose output)
V ?= 0
ifneq ($(V),0)
    VERBOSE := -v
    MAKEFLAGS :=
else
    VERBOSE :=
    MAKEFLAGS += --no-print-directory
endif

.PHONY: all check-toolchain help build-libopencm3 build-examples build-main clean distclean

# Default target
all: check-toolchain build-examples build-main
	@echo "Build complete!"

# Check if ARM toolchain exists
check-toolchain:
	@echo "Checking ARM Toolchain..."
	@if [ ! -d "$(ARM_TOOLCHAIN_DIR)" ]; then \
		echo "ERROR: ARM Toolchain not found at $(ARM_TOOLCHAIN_DIR)"; \
		echo "Run ./setup.sh to download and install the toolchain."; \
		exit 1; \
	fi
	@if [ ! -f "$(ARM_TOOLCHAIN_BIN)/$(ARM_TOOLCHAIN_PREFIX)gcc" ]; then \
		echo "ERROR: ARM toolchain gcc not found at $(ARM_TOOLCHAIN_BIN)/$(ARM_TOOLCHAIN_PREFIX)gcc"; \
		exit 1; \
	fi
	@echo "✓ ARM Toolchain found at: $(ARM_TOOLCHAIN_DIR)"
	@echo "✓ CC: $(CC)"
	@echo "✓ CXX: $(CXX)"

# Build libopencm3 (prerequisite for projects)
build-libopencm3: check-toolchain
	@echo "Building libopencm3..."
	@$(MAKE) -C $(LIBOPENCM3_DIR) lib $(VERBOSE)
	@echo "✓ libopencm3 built successfully"

# Build all examples
build-examples: build-libopencm3
	@echo "Building examples..."
	@for dir in $(SRC_DIR)/examples/*/*/; do \
		if [ -f "$$dir/Makefile" ]; then \
			echo "Building $$dir"; \
			$(MAKE) -C "$$dir" $(VERBOSE) || exit 1; \
		fi; \
	done
	@echo "✓ Examples built successfully"

# Build main projects
build-main: build-libopencm3
	@echo "Building main projects..."
	@for dir in $(SRC_DIR)/main/*/; do \
		if [ -f "$$dir/Makefile" ]; then \
			echo "Building $$dir"; \
			$(MAKE) -C "$$dir" $(VERBOSE) || exit 1; \
		fi; \
	done
	@echo "✓ Main projects built successfully"

# Build specific example: make build-example PROJECT=blink PLATFORM=CDH
# build-example: build-libopencm3
# 	@if [ -z "$(PROJECT)" ] || [ -z "$(PLATFORM)" ]; then \
# 		echo "Usage: make build-example PROJECT=<name> PLATFORM=<CDH|COM>"; \
# 		echo "Available examples:"; \
# 		find $(SRC_DIR)/examples -mindepth 2 -maxdepth 2 -type d ! -name ".*" -exec basename {} \; | sort | uniq; \
# 		exit 1; \
# 	fi
# 	@PROJECT_DIR="$(SRC_DIR)/examples/$(PLATFORM)/$(PROJECT)"; \
# 	if [ ! -d "$$PROJECT_DIR" ]; then \
# 		echo "ERROR: Project not found at $$PROJECT_DIR"; \
# 		exit 1; \
# 	fi
# 	@echo "Building example $(PROJECT) for $(PLATFORM)..."; \
# 	$(MAKE) -C "$$PROJECT_DIR" $(VERBOSE)
# 	@echo "✓ Example $(PROJECT) built successfully"

# Build specific main project: make build-project TARGET=CDH|COM
build-project: build-libopencm3
	@if [ -z "$(TARGET)" ]; then \
		echo "Usage: make build-project TARGET=<CDH|COM>"; \
		exit 1; \
	fi
	@PROJECT_DIR="$(SRC_DIR)/main/$(TARGET)"; \
	if [ ! -d "$$PROJECT_DIR" ]; then \
		echo "ERROR: Project not found at $$PROJECT_DIR"; \
		exit 1; \
	fi
	@echo "Building main project $(TARGET)..."; \
	$(MAKE) -C "$$PROJECT_DIR" $(VERBOSE)
	@echo "✓ Main project $(TARGET) built successfully"

# Clean all build artifacts (but keep toolchain)
clean:
	@echo "Cleaning build artifacts..."
	@$(MAKE) -C $(LIBOPENCM3_DIR) clean
	@find $(SRC_DIR) -name build -type d -exec rm -rf {} + 2>/dev/null || true
	@echo "✓ Cleaned successfully"

# Full distclean (removes everything including dependencies)
distclean: clean
	@echo "Full distclean (removing all generated files)..."
	@find $(SRC_DIR) -name "*.d" -delete
	@find $(SRC_DIR) -name "*.o" -delete
	@find $(SRC_DIR) -name "*.lst" -delete
	@find $(SRC_DIR) -name "*.map" -delete
	@echo "✓ Distclean complete"

# Show help
help:
	@echo "Coral Cubed Flight Project - Build System"
	@echo ""
	@echo "ARM Toolchain Location: $(ARM_TOOLCHAIN_DIR)"
	@echo ""
	@echo "Available Targets:"
	@echo "  make                        - Build everything (libopencm3 + examples + main)"
	@echo "  make build-libopencm3       - Build the libopencm3 library"
	@echo "  make build-examples         - Build all examples"
	@echo "  make build-example          - Build a specific example"
	@echo "                                  make build-example PROJECT=blink PLATFORM=CDH"
	@echo "  make build-main             - Build all main projects"
	@echo "  make build-project          - Build a specific main project"
	@echo "                                  make build-project TARGET=CDH"
	@echo "  make clean                  - Clean build artifacts"
	@echo "  make distclean              - Full clean (removes all generated files)"
	@echo "  make check-toolchain        - Verify ARM toolchain is installed"
	@echo "  make help                   - Show this help message"
	@echo ""
	@echo "Environment Variables:"
	@echo "  V=1                         - Verbose output"
	@echo "  ARM_TOOLCHAIN_PREFIX        - Toolchain prefix: $(ARM_TOOLCHAIN_PREFIX)"
	@echo "  CC                          - C Compiler: $(CC)"
	@echo "  CXX                         - C++ Compiler: $(CXX)"
	@echo ""
	@echo "Project Directories:"
	@echo "  ARM Toolchain:  $(ARM_TOOLCHAIN_DIR)"
	@echo "  libopencm3:     $(LIBOPENCM3_DIR)"
	@echo "  Coral Micro:    $(CORAL_MICRO_DIR)"
	@echo "  Source Root:    $(SRC_DIR)"
	@echo ""
	@echo "Examples:"
	@echo "  make V=1                    # Verbose build"
	@echo "  make clean                  # Clean build artifacts"
	@echo "  make build-example PROJECT=uart PLATFORM=CDH  # Build CDH UART example"
	@echo "  make build-project TARGET=COM                 # Build COM main project"
