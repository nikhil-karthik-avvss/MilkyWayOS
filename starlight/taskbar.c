/*
 * Starlight — Taskbar module
 * Bottom panel with launcher, window buttons, and clock
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
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

    /* Launcher button (highlighted if menu is open) */
    int launcher_w = 110;
    int launcher_h = TASKBAR_BTN_HEIGHT;
    int launcher_y = ty + (TASKBAR_HEIGHT - launcher_h) / 2;

    if (server->desktop.launcher_menu.visible) {
        starlight_draw_rect(fb, 4, launcher_y, launcher_w, launcher_h,
                           TASKBAR_BTN_ACTIVE_R, TASKBAR_BTN_ACTIVE_G,
                           TASKBAR_BTN_ACTIVE_B);
    } else {
        starlight_draw_rect(fb, 4, launcher_y, launcher_w, launcher_h,
                           TASKBAR_BTN_R, TASKBAR_BTN_G, TASKBAR_BTN_B);
    }

    /* Star icon (simple diamond) */
    int star_cx = 16;
    int star_cy = launcher_y + launcher_h / 2;
    starlight_draw_rect(fb, star_cx, star_cy - 3, 1, 7, 200, 180, 255);
    starlight_draw_rect(fb, star_cx - 3, star_cy, 7, 1, 200, 180, 255);
    starlight_draw_rect(fb, star_cx - 1, star_cy - 1, 3, 3, 200, 180, 255);

    starlight_draw_text_simple(fb, 26, launcher_y + 9,
                               "MilkyWayOS", 180, 160, 240);

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

    /* Clock on the right */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char clock_buf[16];
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", t->tm_hour, t->tm_min);

    int clock_x = fb->width - 50;
    int clock_y = ty + 14;
    starlight_draw_text_simple(fb, clock_x, clock_y, clock_buf,
                               160, 150, 200);
}

int starlight_taskbar_hit(struct starlight_server *server, int x, int y) {
    int ty = server->display.mode.vdisplay - TASKBAR_HEIGHT;
    return (y >= ty);
}

int starlight_taskbar_launcher_hit(struct starlight_server *server,
                                    int x, int y) {
    int ty = server->display.mode.vdisplay - TASKBAR_HEIGHT;
    int launcher_y = ty + (TASKBAR_HEIGHT - TASKBAR_BTN_HEIGHT) / 2;
    return (x >= 4 && x < 114 && y >= launcher_y &&
            y < launcher_y + TASKBAR_BTN_HEIGHT);
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
