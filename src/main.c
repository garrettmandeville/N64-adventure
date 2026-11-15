/**
 * @file    main.c
 * @brief   Entry point for the N64 dark fantasy action-adventure.
 *
 * Currently a learning scaffold based on tiny3d's "01_model" example:
 * it initializes the N64 hardware and tiny3d, sets up a camera and
 * lighting, loads a 3D model from the ROM filesystem, and draws it
 * spinning each frame.
 *
 * Program structure (the standard game-loop pattern):
 *   1. Setup  - runs once: hardware, engine, camera, lights, assets.
 *   2. Update - runs every frame: game logic, camera, object placement.
 *   3. Draw   - runs every frame: clear screen, render, present.
 *
 * Built with libdragon + tiny3d. See design/design.md for the game design.
 */

#include <stdio.h>

#include <libdragon.h>          // Core N64 library: display, input, filesystem, RDP.
#include <t3d/t3d.h>            // tiny3d: 3D rendering engine (matrices, viewport, lights).
#include <t3d/t3dmodel.h>       // tiny3d: loading and drawing .t3dm model files.

/** Number of framebuffers (triple buffering): one is shown on screen while
 *  the others are being drawn. Also sizes the per-frame matrix buffers. */
#define FB_COUNT 3

/**
 * @brief  Returns a color that smoothly cycles through the rainbow.
 *
 * Each RGB channel follows a sine wave, offset by a third of a full cycle
 * (2π/3 radians) from the previous one, so the channels peak in turn.
 *
 * @param  s  Phase input; increasing it over time animates the color.
 * @return    An opaque RGBA color.
 */
color_t get_rainbow_color(float s)
{
    // Each sine wave is remapped from [-1, 1] to [0, 1].
    float r = 0.5f + 0.5f * sinf(s);
    float g = 0.5f + 0.5f * sinf(s + 2.0943951023931953f); // 2π/3
    float b = 0.5f + 0.5f * sinf(s + 4.1887902047863905f); // 4π/3

    // Scale [0, 1] to the 8-bit range [0, 255]; alpha is fully opaque.
    return (color_t){(uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255), 255};
}

int main(void)
{
    // =====================================================================
    // SETUP: runs once at power-on
    // =====================================================================

    // --- Debugging & assets ---------------------------------------------
    debug_init_isviewer();          // Route debug output to emulators (ISViewer).
    debug_init_usblog();            // Route debug output to flashcarts over USB.
    asset_init_compression(2);      // Enable reading level-2 compressed assets.
    console_set_debug(true);        // Mirror console output to the debug log.

    // --- Hardware & engine initialization -------------------------------
    dfs_init(DFS_DEFAULT_LOCATION); // Mount the ROM filesystem ("rom:/" paths).
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, FB_COUNT, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    rdpq_init();                    // Start the RDP command queue (pixel drawing).
    t3d_init((T3DInitParams){});    // Start tiny3d with default settings.

    // --- Model transforms -----------------------------------------------
    // Every matrix the RSP reads is allocated uncached: the RSP fetches it
    // from RAM via DMA, so CPU writes must not linger in the CPU cache.

    // Dynamic object: one matrix per framebuffer. The model moves every
    // frame, so the CPU writes a fresh slot each frame and never overwrites
    // a matrix the RSP may still be reading for an earlier frame.
    T3DMat4FP* modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);

    // Static object: a single matrix, written once here during setup.
    // It never changes afterwards, so the RSP can never read it mid-update.
    T3DMat4FP* roomMatFP = malloc_uncached(sizeof(T3DMat4FP));
    t3d_mat4fp_from_srt_euler(roomMatFP,
            (float[3]){0.1f, 0.1f, 0.1f},   // Scale (matches the model's 10%)
            (float[3]){0.0f, 0.0f, 0.0f},   // Rotation (radians, X/Y/Z)
            (float[3]){0.0f, 0.0f, 0.0f}    // Translation (world origin)
        );

    // The camera's view of the world, also buffered once per framebuffer.
    T3DViewport viewport = t3d_viewport_create_buffered(FB_COUNT);

    // --- Camera ---------------------------------------------------------
    // Positions are in world units: X = right, Y = up, Z = toward the viewer.
    // The room spans X/Z -51.2..51.2 and Y 0..64 (floor at Y 0).
    // Camera sits near the front wall, about 5.5 m up, looking slightly down
    // at the gear's center so it is framed in the middle of the screen.
    const fm_vec3_t camPos    = {{0.0f, 35.0f, 40.0f}};     // Camera location.
    const fm_vec3_t camTarget = {{0.0f, 28.0f,  0.0f}};     // Gear center (see Translation below).

    // --- Lighting -------------------------------------------------------
    // Colors are RGBA, 0-255 per channel.
    uint8_t colorAmbient[4] = {80, 80, 100, 0xFF};      // Fill light that reaches every surface.
    uint8_t colorDir[4]     = {0xEE, 0xAA, 0xAA, 0xFF}; // Directional ("sun") light color.
    // Dark violet atmosphere color. Shared by the fog and the screen clear,
    // so distant geometry fades seamlessly into the background.
    color_t fogColor = RGBA32(20, 0, 50, 0xFF);

    // Direction of the directional light. Normalized to length 1, as
    // lighting math expects a pure direction.
    fm_vec3_t lightDirVec = {{-1.0f, 1.0f, 1.0f}};
    fm_vec3_norm(&lightDirVec, &lightDirVec);

    // --- Assets ---------------------------------------------------------
    // Load model geometry and materials from the ROM filesystem.
    // Pipeline: Blender -> .glb -> (gltf_to_t3d) -> .t3dm -> packed into ROM.
    T3DModel *model = t3d_model_load("rom:/model.t3dm");
    T3DModel *room = t3d_model_load("rom:/room.t3dm");

    // --- Loop state -----------------------------------------------------
    float rotAngle = 0.0f;          // Model's current rotation, in radians.
    rspq_block_t *dplDraw = NULL;   // Recorded draw commands (built on first frame).
    rspq_block_t *dplRoom = NULL;   // Recorded draw commands for the static room.
    int frameIdx = 0;               // Which per-frame buffer slot is in use (0..FB_COUNT-1).

    // =====================================================================
    // GAME LOOP: one iteration per frame, runs forever
    // =====================================================================
    for(;;)
    {
        // --- Update -----------------------------------------------------
        frameIdx = (frameIdx + 1) % FB_COUNT;   // Cycle 0, 1, 2, 0, 1, 2...

        // Set the camera lens: field of view, near plane, far plane (draw distance).
        //   Near 10:  the closest visible geometry is the gear's teeth as they
        //             turn toward the camera (~13.8 units away), so 10 never
        //             clips anything.
        //   Far 120:  the farthest visible corner, (+/-51.2, 0, -51.2), is
        //             ~110.3 units from the camera; ~10% margin on top.
        //   A far/near ratio of 12 keeps the 16-bit depth buffer precise.
        //   TODO: Revisit when the camera moves; the room diagonal (~158.3)
        //         covers any camera position inside it.
        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(85.0f), 10.0f, 120.0f);
        // Aim the camera: from camPos, toward camTarget, with +Y as "up".
        // The up vector is a direction only (its length doesn't matter).
        t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(fm_vec3_t){{0.0f, 1.0f, 0.0f}});

        // Advance rotation by a fixed step per frame.
        // TODO: Scale by delta time so speed is independent of frame rate.
        rotAngle -= 0.02f;

        // Build this frame's model matrix from Scale, Rotation (radians,
        // per axis X/Y/Z), and Translation. Written to this frame's slot so
        // the RSP can still safely read the previous frames' matrices.
        float modelScale = 0.1f;    // Source model is authored large; shrink to 10%.
        t3d_mat4fp_from_srt_euler(&modelMatFP[frameIdx],
            (float[3]){modelScale, modelScale, modelScale},   // Scale
            (float[3]){0.0f, rotAngle * 0.2f, rotAngle},        // Rotation
            (float[3]){0.0f, 28.0f, 0.0f}                     // Translation
        );
        // Gear height: its farthest vertex is ~26.8 units from its center
        // (4.19 Blender m x 6.4). It spins on two axes, so any tooth can point
        // straight down; the center must be at least 26.8 above the floor
        // (Y 0). 28 leaves a ~1.2-unit gap.

        // --- Draw -------------------------------------------------------
        // Everything drawn this frame goes between rdpq_attach() and
        // rdpq_detach_show().

        // Acquire the next free framebuffer and the depth buffer, then begin
        // a tiny3d frame rendered through this frame's camera.
        rdpq_attach(display_get(), display_get_zbuf());
        t3d_frame_start();
        t3d_viewport_attach(&viewport);

        // Fog. Must be set after t3d_frame_start(), which disables RDP fog
        // blending every frame, and before anything is drawn.
        //   RDP (per pixel):  blend each pixel toward fogColor by the
        //                     vertex's fog amount (stored in shade alpha).
        //   RSP (per vertex): linear fog ramp in world units, separate from
        //                     the camera's near/far planes.
        rdpq_mode_fog(RDPQ_FOG_STANDARD);
        rdpq_set_fog_color(fogColor);
        // Range chosen by eye for atmosphere: the gear's center (~40.6) stays
        // clear and its far teeth (~67) take on light haze. The back wall's
        // top edge (~95.7) is fully fogged, so it dissolves into the
        // background instead of showing a hard edge.
        t3d_fog_set_range(50.0f, 95.0f);
        t3d_fog_set_enabled(true);

        // Clear the previous frame: background color (shared with the fog)
        // and depth information.
        t3d_screen_clear_color(fogColor);
        t3d_screen_clear_depth();

        // Lighting: ambient fill plus one directional light in slot 0.
        // (The count covers directional lights only; ambient is always on.)
        t3d_light_set_ambient(colorAmbient);
        t3d_light_set_directional(0, colorDir, &lightDirVec);
        t3d_light_set_count(1);

        // Primitive color, read by the model's dynamically colored band.
        // Must be set before the model is drawn.
        rdpq_set_prim_color(get_rainbow_color(rotAngle * 0.42f));

        // Record the model's draw commands once (first frame only) and
        // replay them every frame. The geometry never changes, so this
        // avoids rebuilding the same commands each frame.
        if(!dplDraw) {
            rspq_block_begin();
            t3d_model_draw(model);
            t3d_matrix_pop(1);          // Pairs with the per-frame push below.
            dplDraw = rspq_block_end();
        }

        // Record the static room's draw commands once, as a self-contained
        // block. The room's matrix has a fixed address and never changes
        // after setup, so its push can be recorded inside the block along
        // with the matching pop.
        if(!dplRoom) {
            rspq_block_begin();
            t3d_matrix_push(roomMatFP);
            t3d_model_draw(room);
            t3d_matrix_pop(1);          // Pairs with the recorded push above.
            dplRoom = rspq_block_end();
        }

        // Spinning model: apply this frame's matrix, then replay its block.
        // The push stays outside the recording because the matrix slot
        // changes every frame; the matching pop is recorded inside the block.
        t3d_matrix_push(&modelMatFP[frameIdx]);
        rspq_block_run(dplDraw);

        // Room: self-contained (push and pop are recorded inside), so it
        // needs no setup here and could run in either order; the depth
        // buffer resolves which surfaces are in front.
        rspq_block_run(dplRoom);

        // Finish the frame and queue it for display.
        rdpq_detach_show();
    }

    // =====================================================================
    // SHUTDOWN: unreachable while the game loop runs forever; kept for
    // completeness.
    // =====================================================================
    t3d_destroy();
    return 0;

}
