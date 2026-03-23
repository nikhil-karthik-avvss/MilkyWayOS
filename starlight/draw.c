/*
 * Starlight — Drawing module
 * Simple 2D drawing primitives
 */

#include <string.h>
#include "starlight.h"

void starlight_draw_clear(struct starlight_framebuffer *fb,
                          uint8_t r, uint8_t g, uint8_t b) {
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            uint32_t off = y * fb->stride + x * 4;
            fb->map[off + 0] = b;
            fb->map[off + 1] = g;
            fb->map[off + 2] = r;
            fb->map[off + 3] = 0xFF;
        }
    }
}

void starlight_draw_rect(struct starlight_framebuffer *fb,
                         int x, int y, int w, int h,
                         uint8_t r, uint8_t g, uint8_t b) {
    for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        if (py < 0 || py >= (int)fb->height) continue;
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            if (px < 0 || px >= (int)fb->width) continue;
            uint32_t off = py * fb->stride + px * 4;
            fb->map[off + 0] = b;
            fb->map[off + 1] = g;
            fb->map[off + 2] = r;
            fb->map[off + 3] = 0xFF;
        }
    }
}

/* Draw a simple arrow cursor */
void starlight_draw_cursor(struct starlight_framebuffer *fb,
                           int cx, int cy) {
    /* Simple triangular cursor shape */
    static const uint8_t cursor_data[CURSOR_SIZE][CURSOR_SIZE] = {
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0},
        {1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,1,2,2,1,0,0,0,0,0,0,0,0},
        {1,1,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
        {1,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0},
    };

    for (int dy = 0; dy < CURSOR_SIZE; dy++) {
        for (int dx = 0; dx < CURSOR_SIZE; dx++) {
            int px = cx + dx;
            int py = cy + dy;
            if (px < 0 || px >= (int)fb->width) continue;
            if (py < 0 || py >= (int)fb->height) continue;

            uint8_t val = cursor_data[dy][dx];
            if (val == 1) {
                /* Black outline */
                uint32_t off = py * fb->stride + px * 4;
                fb->map[off + 0] = 0;
                fb->map[off + 1] = 0;
                fb->map[off + 2] = 0;
                fb->map[off + 3] = 0xFF;
            } else if (val == 2) {
                /* White/light blue fill */
                uint32_t off = py * fb->stride + px * 4;
                fb->map[off + 0] = STARLIGHT_CURSOR_B;
                fb->map[off + 1] = STARLIGHT_CURSOR_G;
                fb->map[off + 2] = STARLIGHT_CURSOR_R;
                fb->map[off + 3] = 0xFF;
            }
        }
    }
}
