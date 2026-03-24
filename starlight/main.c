/*
 * Starlight Display Server v0.6 — MilkyWayOS
 * With Nebula desktop environment and Pulsar terminal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <libinput.h>
#include <unistd.h>
#include "starlight.h"

int main(void) {
    printf("=== Starlight Display Server v0.6 ===\n");
    printf("=== MilkyWayOS — Nebula Desktop ===\n\n");

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

    /* Initialize Nebula desktop */
    nebula_init(&server.desktop,
                server.display.mode.hdisplay,
                server.display.mode.vdisplay);

    /* Open initial terminal */
    pulsar_create_window(&server, 150, 100, 550, 380);

    printf("[Starlight] Running — Nebula Desktop\n");
    printf("[Starlight] Right-click desktop for menu\n");
    printf("[Starlight] Click MilkyWayOS for launcher\n");
    printf("[Starlight] Ctrl+Shift+T = new terminal\n");
    printf("[Starlight] Ctrl+Shift+Q = quit\n");

    int li_fd = libinput_get_fd(server.input.li);

    while (server.running) {
        struct pollfd pfds[MAX_WINDOWS + 1];
        int nfds = 0;

        pfds[nfds].fd = li_fd;
        pfds[nfds].events = POLLIN;
        nfds++;

        for (int i = 0; i < server.window_count; i++) {
            struct starlight_window *win = &server.windows[i];
            if (win->alive && win->terminal && win->terminal->active) {
                pfds[nfds].fd = win->terminal->master_fd;
                pfds[nfds].events = POLLIN;
                nfds++;
            }
        }

        poll(pfds, nfds, 16);

        starlight_input_process(&server);

        for (int i = 0; i < server.window_count; i++) {
            struct starlight_window *win = &server.windows[i];
            if (win->alive && win->terminal && win->terminal->active) {
                pulsar_process_output(win->terminal);
            }
        }

        /* Draw */
        struct starlight_framebuffer *fb =
            starlight_display_back_buffer(&server.display);

        /* Clear + wallpaper */
        starlight_draw_clear(fb, STARLIGHT_BG_R, STARLIGHT_BG_G,
                            STARLIGHT_BG_B);
        nebula_draw_wallpaper(fb, &server.desktop);

        /* Draw windows */
        for (int i = 0; i < server.window_count; i++) {
            int idx = server.window_order[i];
            struct starlight_window *win = &server.windows[idx];
            if (!win->alive || !win->visible) continue;
            starlight_draw_window(fb, win, (idx == server.focus));
        }

        /* Draw about text inside about windows */
        nebula_draw_about(fb, &server);

        /* Taskbar */
        starlight_draw_taskbar(fb, &server);

        /* Menus (on top of everything except cursor) */
        nebula_draw_menu(fb, &server.desktop.desktop_menu,
                        (int)server.input.cursor_x,
                        (int)server.input.cursor_y);
        nebula_draw_menu(fb, &server.desktop.launcher_menu,
                        (int)server.input.cursor_x,
                        (int)server.input.cursor_y);

        /* Cursor always on top */
        starlight_draw_cursor(fb, (int)server.input.cursor_x,
                             (int)server.input.cursor_y);

        starlight_display_swap(&server.display);
    }

    for (int i = 0; i < server.window_count; i++) {
        if (server.windows[i].terminal) {
            pulsar_destroy(server.windows[i].terminal);
            free(server.windows[i].terminal);
        }
    }

    starlight_input_destroy(&server.input);
    starlight_display_destroy(&server.display);

    printf("[Starlight] Shut down cleanly.\n");
    return 0;
}
