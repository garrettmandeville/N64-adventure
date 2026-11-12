# =============================================================================
# Makefile: N64 dark fantasy action-adventure
#
# Builds the game ROM (.z64) using libdragon's build rules.
# Run from the project root with:   libdragon make
# Clean build outputs with:         libdragon make clean
# =============================================================================

# Verbose output: print full compiler commands (useful for debugging builds).
V=1

# Where source files live, and where compiled outputs go.
SOURCE_DIR=src
BUILD_DIR=build

# libdragon's standard N64 build rules (compiler, linker, ROM packaging).
# N64_INST is set automatically inside the libdragon toolchain container.
include $(N64_INST)/include/n64.mk

# tiny3d build rules: adds tiny3d's headers and library to the build, and
# defines T3D_GLTF_TO_3D (the .glb -> .t3dm model converter).
T3D_INST=$(shell realpath tiny3d)
include $(T3D_INST)/t3d.mk

# tiny3d requires a newer C standard (C23 with GNU extensions).
N64_CFLAGS += -std=gnu2x

# Default target: build the ROM.
all: hello.z64
.PHONY: all

# Object files to link into the game (one per .c file in src/).
OBJS = $(BUILD_DIR)/main.o

# -----------------------------------------------------------------------------
# Asset pipeline
#   assets/      source art (edit these)
#   filesystem/  N64-ready converted files (generated; packed into the ROM
#                and readable at runtime via "rom:/<name>")
# -----------------------------------------------------------------------------

# Source assets, discovered automatically by file extension.
assets_png  = $(wildcard assets/*.png)
assets_gltf = $(wildcard assets/*.glb)

# Converted output files: same names, moved to filesystem/ with N64 extensions.
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite))) \
              $(addprefix filesystem/,$(notdir $(assets_gltf:%.glb=%.t3dm)))

# Textures: .png -> .sprite (N64 texture format).
filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

# Models: .glb (Blender export) -> .t3dm (tiny3d model), then compressed at
# level 2 to match asset_init_compression(2) in src/main.c.
filesystem/%.t3dm: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [T3D-MODEL] $@"
	$(T3D_GLTF_TO_3D) "$<" $@
	$(N64_BINDIR)/mkasset -c 2 -o filesystem $@

# -----------------------------------------------------------------------------
# ROM
# -----------------------------------------------------------------------------

# Internal ROM header title.
hello.z64: N64_ROM_TITLE="Hello World"

# The ROM includes a filesystem image (.dfs), built from every converted
# asset. (The packing recipe itself is provided by n64.mk.)
hello.z64: $(BUILD_DIR)/hello.dfs
$(BUILD_DIR)/hello.dfs: $(assets_conv)

# The ELF (executable) is linked from the object files, then packed into the ROM.
$(BUILD_DIR)/hello.elf: $(OBJS)

# Remove all build outputs and generated assets (assets/ is never touched).
clean:
	rm -f $(BUILD_DIR)/* *.z64
	rm -rf filesystem
.PHONY: clean



# Auto-generated header dependencies: rebuild a .c file when a header it
# includes changes.
-include $(wildcard $(BUILD_DIR)/*.d)
