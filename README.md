# N64 Adventure *(working title)*

A 3D dark-fantasy action-adventure for the **Nintendo 64**, written in C and running on real N64 hardware and accurate emulators.

The player is a hero who looks like a villain. They fight with sword and shield, and **every weapon and shield holds its own dark spell**. Those spells are also the tools for exploration and puzzles, so finding new gear is how the world opens up. The full design is in [design/design.md](design/design.md).

> **Status:** early development. A textured, lit model spins inside a stone room, with the camera and depth range fitted to the room. Fog is next. See the [roadmap](#roadmap).

---

## Tech stack

| | |
|---|---|
| **Language** | C (GNU C23) |
| **Platform** | Nintendo 64 (MIPS VR4300 CPU, RSP + RDP graphics coprocessors) |
| **SDK** | [libdragon](https://github.com/DragonMinded/libdragon) (`preview` branch), an open-source N64 SDK |
| **3D renderer** | [tiny3d](https://github.com/HailToDodongo/tiny3d), a 3D graphics library for libdragon |
| **Art** | Blender → glTF (`.glb`) → tiny3d model format (`.t3dm`) |
| **Build** | GNU Make with libdragon's build rules |

## What I'm learning

I'm building this project to learn C and low-level programming on real, constrained hardware. Concepts I've worked through so far:

- **Parallel processors:** the CPU builds frame *N+1* while the RSP is still processing frame *N*. Anything the RSP reads must not change under it, so per-frame data is buffered once per framebuffer.
- **Cache coherency and DMA:** the RSP copies data from main RAM by DMA and cannot see the CPU's cache, so shared data like transform matrices lives in uncached memory.
- **Transform matrices:** scale, rotation and translation combined into one 4×4 fixed-point matrix (the RSP has no floating-point unit). Static objects need one matrix written once; moving objects need one per framebuffer.
- **Command queues and recorded display lists:** draw calls queue commands for the RSP rather than drawing immediately, and unchanging command sequences are recorded once and replayed each frame. A recording stores the *address* of a matrix, not its contents, so a static object's push can be recorded inside its block, while a moving object's per-frame matrix has to be pushed outside it.
- **The matrix stack:** every push must be balanced by exactly one pop. Unbalanced stacks compile fine and break at runtime.
- **Framebuffers:** double and triple buffering let the CPU, RSP and RDP work on different frames at once without showing a half-drawn image, trading memory (~150 KB per 320×240 buffer) for less waiting.
- **Depth-buffer precision:** a 16-bit depth buffer spreads 65,536 values between the near and far planes, packed most densely near the camera. Fitting the planes tightly to the scene (a small far/near ratio) prevents z-fighting.
- **Vector math:** distance as √(dx² + dy² + dz²), direction vs. magnitude, and using it to fit the camera's view range and keep a spinning model clear of the floor.
- **Model origins:** levels have their origin at floor level and props at their base, so positions, rotation and collision start from a clean reference.
- **Working within hardware limits:** 4 KB of texture memory, power-of-two texture wrapping, `float` over `double`, and a 16-bit depth buffer.

## How I'm building this

I write all of the game's code myself. I use Claude (Anthropic's AI assistant) as a **mentor**, not a code generator:

- It **explains** hardware and C concepts with small examples that aren't my exact solution, and then I write the code.
- When I'm debugging, it **asks guiding questions** first, so I learn to read compiler errors and headers myself.
- When I finish a step, it **reviews** my code honestly: what works, what to fix, and what to learn next.

It has also handled tooling and support work: setting up the toolchain and build configuration, generating and re-exporting the placeholder room model, and drafting documentation. On one occasion, at my request, it wrote the camera, depth-plane and model-placement values for the room, with a full explanation of the math behind each number. Commits where Claude made changes are marked with a `Co-Authored-By` line. These rules live in [CLAUDE.md](CLAUDE.md), so the same approach applies on every machine I work from.

## Building

### Requirements

- The **libdragon toolchain** (`mips64-elf-gcc`) with libdragon's `preview` branch and tiny3d built and installed into it. See the [libdragon installation guide](https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon).
- The `N64_INST` environment variable pointing at that toolchain, with `$N64_INST/bin` on your `PATH`.
- An N64 emulator with accurate RSP emulation, such as [ares](https://ares-emu.net/), or a flashcart for real hardware.

### Steps

```bash
# Clone with the libdragon and tiny3d submodules
git clone --recurse-submodules https://github.com/garrettmandeville/N64-adventure.git
cd N64-adventure

# Build libdragon and tiny3d into $N64_INST (first time only)
(cd libdragon && ./build.sh)
(cd tiny3d && ./build.sh)

# Build the ROM
make
```

This produces `hello.z64`, which you can open in your emulator.

### Alternative: Docker (macOS)

The [libdragon CLI](https://github.com/anacierdem/libdragon-docker) runs the toolchain inside a Docker container, so nothing needs to be installed on the host apart from Docker and the CLI. Once the container has libdragon (`preview`) and tiny3d installed, prefix the build commands with `libdragon`:

```bash
libdragon make          # build
libdragon make clean    # clean
```

See [CLAUDE.md](CLAUDE.md) for this project's exact macOS setup.

## Project structure

```
src/          Game source code (C)
assets/       Source art: Blender files, glTF models, PNG textures
design/       Game design document
libdragon/    N64 SDK (git submodule)
tiny3d/       3D renderer (git submodule)
Makefile      Builds the ROM and converts assets
```

Converted assets (`filesystem/`) and build output (`build/`, `*.z64`) are generated by `make` and not committed.

## Roadmap

From the [design document](design/design.md#13-build-milestones):

- [ ] **A model on screen:** tiny3d renders a 3D model in a room *(in progress)*
- [ ] **A moving character:** analog movement, character faces its direction
- [ ] **Follow camera:** orbiting camera with C-button control
- [ ] **Collision:** floors and walls
- [ ] **Animation:** idle, run and attack (skeletal animation)
- [ ] **Lock-on and combat:** Z-targeting, sword swing, shield guard and counter
- [ ] **Magic system:** magic bar, weapon and shield spells
- [ ] **One complete area:** a small field with enemies
- [ ] **Connected areas:** transitions between areas

## License

This project is released under the [MIT License](LICENSE). The bundled submodules keep their own licenses: [libdragon](https://github.com/DragonMinded/libdragon) is public domain (the Unlicense), and [tiny3d](https://github.com/HailToDodongo/tiny3d) is MIT.

## Acknowledgements

Built on [libdragon](https://github.com/DragonMinded/libdragon) and [tiny3d](https://github.com/HailToDodongo/tiny3d), with thanks to the N64 homebrew community.
