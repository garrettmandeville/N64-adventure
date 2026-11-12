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

    // --- Per-frame transform buffers ------------------------------------
    // One model matrix per framebuffer, so the CPU never overwrites a matrix
    // the RSP is still reading. Uncached because the RSP reads it via DMA.
    T3DMat4FP* modelMatFP = malloc_uncached(sizeof(T3DMat4FP) * FB_COUNT);

    // The camera's view of the world, also buffered once per framebuffer.
    T3DViewport viewport = t3d_viewport_create_buffered(FB_COUNT);

    // --- Camera ---------------------------------------------------------
    // Positions are in world units: X = right, Y = up, Z = toward the viewer.
    const fm_vec3_t camPos = {{0,10.0f,40.0f}};     // Camera location.
    const fm_vec3_t camTarget = {{0,0,0}};          // Point the camera looks at (world origin).

    // --- Lighting -------------------------------------------------------
    // Colors are RGBA, 0-255 per channel.
    uint8_t colorAmbient[4] = {80, 80, 100, 0xFF};      // Fill light that reaches every surface.
    uint8_t colorDir[4]     = {0xEE, 0xAA, 0xAA, 0xFF}; // Directional ("sun") light color.

    // Direction of the directional light. Normalized to length 1, as
    // lighting math expects a pure direction.
    fm_vec3_t lightDirVec = {{-1.0f, 1.0f, 1.0f}};
    fm_vec3_norm(&lightDirVec, &lightDirVec);

    // --- Assets ---------------------------------------------------------
    // Load model geometry and materials from the ROM filesystem.
    // Pipeline: Blender -> .glb -> (gltf_to_t3d) -> .t3dm -> packed into ROM.
    T3DModel *model = t3d_model_load("rom:/model.t3dm");

    // --- Loop state -----------------------------------------------------
    float rotAngle = 0.0f;          // Model's current rotation, in radians.
    rspq_block_t *dplDraw = NULL;   // Recorded draw commands (built on first frame).
    int frameIdx = 0;               // Which per-frame buffer slot is in use (0..FB_COUNT-1).

    // =====================================================================
    // GAME LOOP: one iteration per frame, runs forever
    // =====================================================================
    for(;;)
    {
        // --- Update -----------------------------------------------------
        frameIdx = (frameIdx + 1) % FB_COUNT;   // Cycle 0, 1, 2, 0, 1, 2...

        // Set the camera lens: field of view, near plane, far plane (draw distance).
        t3d_viewport_set_projection(&viewport, T3D_DEG_TO_RAD(85.0f), 10.0f, 150.0f);
        // Aim the camera: from camPos, toward camTarget, with +Y as "up".
        t3d_viewport_look_at(&viewport, &camPos, &camTarget, &(fm_vec3_t){{0,1,0}});

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
            (float[3]){0, 0, 0}                                 // Translation (world origin)
        );

        // --- Draw -------------------------------------------------------
        // Everything drawn this frame goes between rdpq_attach() and
        // rdpq_detach_show().

        // Acquire the next free framebuffer and the depth buffer, then begin
        // a tiny3d frame rendered through this frame's camera.
        rdpq_attach(display_get(), display_get_zbuf());
        t3d_frame_start();
        t3d_viewport_attach(&viewport);

        // Clear the previous frame: background color and depth information.
        t3d_screen_clear_color(RGBA32(100, 80, 80, 0xFF));
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

        // Apply this frame's model matrix, then replay the recorded draw.
        // The push stays outside the recording because the matrix changes
        // every frame; the matching pop is recorded inside the block.
        t3d_matrix_push(&modelMatFP[frameIdx]);
        rspq_block_run(dplDraw);

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
