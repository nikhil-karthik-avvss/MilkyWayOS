/*
 * Starlight Display Server v0.3 — MilkyWayOS
 * Window management with click-to-focus and dragging
 */

#include <stdio.h>
#include <poll.h>
#include <libinput.h>
#include <unistd.h>
#include "starlight.h"

int main(void) {
    printf("=== Starlight Display Server v0.3 ===\n");
    printf("=== MilkyWayOS ===\n\n");

    struct starlight_server server = { 0 };
    server.running = 1;
    server.focus = -1;

    if (starlight_display_init(&server.display) < 0) {
        fprintf(stderr, "[Starlight] Display init failed\n");
        return 1;
    }

    if (starlight_input_init(&server.input) < 0) {
        fprintf(stderr, "[Starlight] Input init failed\n");
        starlight_display_destroy(&server.display);
        return 1;
    }

    server.input.cursor_x = server.display.mode.hdisplay / 2.0;
    server.input.cursor_y = server.display.mode.vdisplay / 2.0;

    /* Create demo windows */
    starlight_window_create(&server, 100, 100, 300, 200,
                           "Welcome to MilkyWayOS", 20, 20, 50);
    starlight_window_create(&server, 250, 180, 280, 180,
                           "Nebula Desktop", 30, 15, 45);
    starlight_window_create(&server, 450, 120, 260, 220,
                           "Starlight v0.3", 15, 25, 40);

    printf("[Starlight] Running — drag windows, click to focus, Escape to exit\n");

    int li_fd = libinput_get_fd(server.input.li);

    while (server.running) {
        struct pollfd pfd = { .fd = li_fd, .events = POLLIN };
        poll(&pfd, 1, 16);

        starlight_input_process(&server);

        struct starlight_framebuffer *fb =
            starlight_display_back_buffer(&server.display);

        /* Clear background */
        starlight_draw_clear(fb, STARLIGHT_BG_R, STARLIGHT_BG_G,
                            STARLIGHT_BG_B);

        /* Draw windows in order (back to front) */
        for (int i = 0; i < server.window_count; i++) {
            int idx = server.window_order[i];
            struct starlight_window *win = &server.windows[idx];
            if (!win->alive || !win->visible) continue;
            starlight_draw_window(fb, win, (idx == server.focus));
        }

        /* Cursor always on top */
        starlight_draw_cursor(fb, (int)server.input.cursor_x,
                             (int)server.input.cursor_y);

        starlight_display_swap(&server.display);
    }

    starlight_input_destroy(&server.input);
    starlight_display_destroy(&server.display);

    printf("[Starlight] Shut down cleanly.\n");
    return 0;
}
