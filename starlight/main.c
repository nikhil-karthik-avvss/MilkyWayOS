/*
 * Starlight Display Server v0.7 — MilkyWayOS
 * With Nebula desktop, Pulsar terminal, and Nova file manager
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <libinput.h>
#include <unistd.h>
#include "starlight.h"

int main(void) {
    printf("=== Starlight Display Server v0.7 ===\n");
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

    nebula_init(&server.desktop,
                server.display.mode.hdisplay,
                server.display.mode.vdisplay);

    /* Open initial terminal */
    pulsar_create_window(&server, 150, 100, 550, 380);

    printf("[Starlight] Running — Nebula Desktop\n");
    printf("[Starlight] Right-click desktop for menu\n");
    printf("[Starlight] Ctrl+Shift+T = new terminal\n");
    printf("[Starlight] Ctrl+Shift+F = new file manager\n");
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

        /* Update Nova hover for all file manager windows */
        int cx = (int)server.input.cursor_x;
        int cy = (int)server.input.cursor_y;
        for (int i = 0; i < server.window_count; i++) {
            struct starlight_window *win = &server.windows[i];
            if (win->alive && win->filemanager && win->filemanager->active) {
                /* Update hover tracking with cursor position */
                /* nova_draw will handle hover detection using these coords */
            }
        }

        /* Draw */
        struct starlight_framebuffer *fb =
            starlight_display_back_buffer(&server.display);

        starlight_draw_clear(fb, STARLIGHT_BG_R, STARLIGHT_BG_G,
                            STARLIGHT_BG_B);
        nebula_draw_wallpaper(fb, &server.desktop);

        /* Draw windows — pass cursor for Nova hover */
        for (int i = 0; i < server.window_count; i++) {
            int idx = server.window_order[i];
            struct starlight_window *win = &server.windows[idx];
            if (!win->alive || !win->visible) continue;

            /* Draw window chrome and content */
            int bx = win->x - WINDOW_BORDER;
            int by = win->y - TITLEBAR_HEIGHT - WINDOW_BORDER;
            int bw = win->width + WINDOW_BORDER * 2;
            int bh = win->height + TITLEBAR_HEIGHT + WINDOW_BORDER * 2;
            int focused = (idx == server.focus);

            /* Border */
            starlight_draw_rect(fb, bx, by, bw, bh,
                                BORDER_R, BORDER_G, BORDER_B);

            /* Titlebar */
            if (focused) {
                starlight_draw_rect(fb, win->x, win->y - TITLEBAR_HEIGHT,
                                   win->width, TITLEBAR_HEIGHT,
                                   TITLEBAR_ACTIVE_R, TITLEBAR_ACTIVE_G,
                                   TITLEBAR_ACTIVE_B);
            } else {
                starlight_draw_rect(fb, win->x, win->y - TITLEBAR_HEIGHT,
                                   win->width, TITLEBAR_HEIGHT,
                                   TITLEBAR_R, TITLEBAR_G, TITLEBAR_B);
            }

            starlight_draw_text_simple(fb, win->x + 8,
                                       win->y - TITLEBAR_HEIGHT + 10,
                                       win->title, 220, 220, 240);

            starlight_draw_rect(fb, win->x + win->width - 20,
                                win->y - TITLEBAR_HEIGHT + 4, 16, 20,
                                CLOSE_BTN_R, CLOSE_BTN_G, CLOSE_BTN_B);
            starlight_draw_text_simple(fb, win->x + win->width - 17,
                                       win->y - TITLEBAR_HEIGHT + 7,
                                       "X", 255, 255, 255);

            /* Content area */
            starlight_draw_rect(fb, win->x, win->y, win->width, win->height,
                                win->bg_r, win->bg_g, win->bg_b);

            /* Draw app content */
            if (win->terminal) {
                pulsar_draw(fb, win);
            } else if (win->filemanager) {
                nova_draw(fb, win, cx, cy);
            }
        }

        nebula_draw_about(fb, &server);
        starlight_draw_taskbar(fb, &server);

        nebula_draw_menu(fb, &server.desktop.desktop_menu, cx, cy);
        nebula_draw_menu(fb, &server.desktop.launcher_menu, cx, cy);

        starlight_draw_cursor(fb, cx, cy);

        starlight_display_swap(&server.display);
    }

    for (int i = 0; i < server.window_count; i++) {
        if (server.windows[i].terminal) {
            pulsar_destroy(server.windows[i].terminal);
            free(server.windows[i].terminal);
        }
        if (server.windows[i].filemanager) {
            nova_destroy(server.windows[i].filemanager);
            free(server.windows[i].filemanager);
        }
    }

    starlight_input_destroy(&server.input);
    starlight_display_destroy(&server.display);

    printf("[Starlight] Shut down cleanly.\n");
    return 0;
}
