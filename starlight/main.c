/*
 * Starlight Display Server v0.2 — MilkyWayOS
 * Now with mouse cursor and keyboard input!
 */

#include <stdio.h>
#include <poll.h>
#include <libinput.h>
#include <unistd.h>
#include "starlight.h"

int main(void) {
    printf("=== Starlight Display Server v0.2 ===\n");
    printf("=== MilkyWayOS ===\n\n");

    struct starlight_server server = { .running = 1 };

    /* Initialize display */
    if (starlight_display_init(&server.display) < 0) {
        fprintf(stderr, "[Starlight] Display init failed\n");
        return 1;
    }

    /* Initialize input */
    if (starlight_input_init(&server.input) < 0) {
        fprintf(stderr, "[Starlight] Input init failed\n");
        starlight_display_destroy(&server.display);
        return 1;
    }

    /* Center cursor */
    server.input.cursor_x = server.display.mode.hdisplay / 2.0;
    server.input.cursor_y = server.display.mode.vdisplay / 2.0;

    printf("[Starlight] Running — press Escape to exit\n");

    int li_fd = libinput_get_fd(server.input.li);

    /* Main loop */
    while (server.running) {
        struct pollfd pfd = {
            .fd = li_fd,
            .events = POLLIN,
        };

        poll(&pfd, 1, 16);  /* ~60fps */

        /* Process input */
        starlight_input_process(&server);

        /* Draw to back buffer */
        struct starlight_framebuffer *fb =
            starlight_display_back_buffer(&server.display);

        starlight_draw_clear(fb, STARLIGHT_BG_R, STARLIGHT_BG_G,
                            STARLIGHT_BG_B);
        starlight_draw_cursor(fb, (int)server.input.cursor_x,
                             (int)server.input.cursor_y);

        /* Swap buffers */
        starlight_display_swap(&server.display);
    }

    /* Cleanup */
    starlight_input_destroy(&server.input);
    starlight_display_destroy(&server.display);

    printf("[Starlight] Shut down cleanly.\n");
    return 0;
}
