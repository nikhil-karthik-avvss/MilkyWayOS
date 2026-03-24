/*
 * Starlight — Taskbar module
 * Bottom panel showing open windows
 */

#include <stdio.h>
#include <string.h>
#include "starlight.h"

void starlight_draw_taskbar(struct starlight_framebuffer *fb,
                            struct starlight_server *server) {
    int ty = fb->height - TASKBAR_HEIGHT;

    /* Taskbar background */
    starlight_draw_rect(fb, 0, ty, fb->width, TASKBAR_HEIGHT,
                        TASKBAR_R, TASKBAR_G, TASKBAR_B);

    /* Top border line */
    starlight_draw_rect(fb, 0, ty, fb->width, 1,
                        BORDER_R, BORDER_G, BORDER_B);

    /* MilkyWayOS branding */
    starlight_draw_text_simple(fb, 10, ty + 14,
                               "MilkyWayOS", 140, 120, 200);

    /* Window buttons */
    int bx = TASKBAR_START_X;
    for (int i = 0; i < server->window_count; i++) {
        struct starlight_window *win = &server->windows[i];
        if (!win->alive) continue;

        int by = ty + (TASKBAR_HEIGHT - TASKBAR_BTN_HEIGHT) / 2;
        int focused = (i == server->focus);

        if (focused) {
            starlight_draw_rect(fb, bx, by,
                               TASKBAR_BTN_WIDTH, TASKBAR_BTN_HEIGHT,
                               TASKBAR_BTN_ACTIVE_R, TASKBAR_BTN_ACTIVE_G,
                               TASKBAR_BTN_ACTIVE_B);
        } else {
            starlight_draw_rect(fb, bx, by,
                               TASKBAR_BTN_WIDTH, TASKBAR_BTN_HEIGHT,
                               TASKBAR_BTN_R, TASKBAR_BTN_G,
                               TASKBAR_BTN_B);
        }

        char label[20];
        strncpy(label, win->title, 19);
        label[19] = '\0';

        starlight_draw_text_simple(fb, bx + 8, by + 9,
                                   label, 200, 200, 220);

        bx += TASKBAR_BTN_WIDTH + TASKBAR_BTN_MARGIN;
    }
}

int starlight_taskbar_hit(struct starlight_server *server, int x, int y) {
    int ty = server->display.mode.vdisplay - TASKBAR_HEIGHT;
    return (y >= ty);
}

int starlight_taskbar_window_at(struct starlight_server *server,
                                int x, int y) {
    int ty = server->display.mode.vdisplay - TASKBAR_HEIGHT;
    int by = ty + (TASKBAR_HEIGHT - TASKBAR_BTN_HEIGHT) / 2;

    if (y < by || y > by + TASKBAR_BTN_HEIGHT) return -1;

    int bx = TASKBAR_START_X;
    for (int i = 0; i < server->window_count; i++) {
        if (!server->windows[i].alive) continue;

        if (x >= bx && x < bx + TASKBAR_BTN_WIDTH) {
            return i;
        }
        bx += TASKBAR_BTN_WIDTH + TASKBAR_BTN_MARGIN;
    }

    return -1;
}
