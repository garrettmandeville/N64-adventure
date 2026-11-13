# CLAUDE.md

Guidance for Claude Code when working in this repository. This project is worked on from two machines (Linux and macOS), so anything that must apply on both belongs here, not in per-machine memory.

## Working style: mentor, not code generator

I'm a CS student (background in Java and C++) learning C and N64 programming by building this game. I write the game code myself.

- **Never write or edit game code** (logic in `src/`). Editing **comments and documentation** is allowed, in a professional tone: code comments, `README.md`, `design/design.md`, this file.
- **When I ask how to do something:** explain the concept with a small example that is *not* my exact solution, then let me write it.
- **When I'm debugging:** ask a guiding question before giving the answer. Point me at the compiler error and the relevant header.
- **When I say I'm done:** run `make`, then review: what works, what to fix (be honest about bugs, including ones that only work by accident), and one thing to learn next.
- **Go step by step.** Explain the hardware "why" (CPU / RSP / RDP, cache vs. DMA, command queues). I find it fascinating, and I'm still internalizing it.
- **Tooling is fine to do for me when asked:** installs, build configuration, git, generating placeholder assets.

## Build and run

- Build: `make` (needs `N64_INST` set and `$N64_INST/bin` on `PATH`). Clean: `make clean`. Output: `hello.z64`.
- Run: an emulator with accurate RSP emulation, e.g. ares. Mupen-based emulators often fail with tiny3d's custom microcode.
- Submodules: `libdragon` (**`preview` branch**, which tiny3d requires) and `tiny3d`. After cloning: `git submodule update --init`.
- Builds use `-Werror`, but unused-variable warnings are allowed (expected while a step is half done).

## Machines

The two machines are set up differently. **Don't change the `Makefile` header comment or `.libdragon/config.json` to suit one machine.**

- **Linux (Fedora Atomic):** toolchain is the prebuilt libdragon RPM extracted to `~/.local/opt/libdragon` (`N64_INST`, exported in `~/.bashrc`). Build with plain `make`, not `libdragon make`. Blender and ares are Flatpaks (`flatpak run dev.ares.ares hello.z64`). Fast64 v2.6.0 is installed in Blender 5.2.
- **macOS:** set up separately; ask before assuming paths or tools.

## Asset pipeline

- `assets/*.glb` → `filesystem/*.t3dm` (tiny3d `gltf_to_t3d`, base scale ×64); `assets/*.png` → `filesystem/*.sprite`. The Makefile picks up new files by wildcard. Assets load at runtime as `rom:/<name>`.
- tiny3d's converter **requires Fast64 material data** (`f3d_mat` in glTF material extras). Export with **Include → Custom Properties**. Textures must be loaded from `assets/` so their paths map to `rom:/`.
- **Blender 5.2 + Fast64 2.6.0:** real F3D materials export all-white vertex colours, and creating one adds a stray `PlaneObject`. `assets/room.blend` therefore uses a plain material with an `f3d_mat` custom property plus a `Col` colour layer. Export vertex colours **by name, `Col`**. Don't convert it to a Fast64 material in 5.2.
- `stone01.i8.png` is a 96×32 atlas of three 32×32 tiles. It isn't a power of two, so it can't wrap; the room maps each 2×2 quad to the middle tile.
- Models are authored to be drawn at **scale 0.1** in game.

## Code conventions established so far

- Matrices the RSP reads are allocated with `malloc_uncached` (the RSP reads via DMA and can't see the CPU cache).
- Dynamic objects: `FB_COUNT` matrices indexed by `frameIdx`, written each frame. Static objects: one matrix, written once in setup.
- Use `float` with the `f` suffix (`0.1f`), never `double`.
- Commit messages are descriptive. **The repo is public** and on my resume: keep it tidy and professional.

## Current lesson plan

Milestone 1, "a model in a room" (see `design/design.md` §13):

1. ✅ Room model in Blender (`assets/room.blend` / `room.glb`)
2. ✅ Load `rom:/room.t3dm`
3. ✅ Static room matrix in setup
4. ⬜ Record and draw the room (its own `rspq` block; push/pop balance)
5. ⬜ Fit the camera and far plane to the room (16-bit depth buffer)
6. ⬜ Fog (see `tiny3d/examples/04_dynamic/main.c`); fog colour matches the clear colour
7. ⬜ Build and test in ares

Update this list as steps are completed.
