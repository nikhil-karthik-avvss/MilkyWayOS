/*
 * Starlight — Input module
 * Handles mouse and keyboard via libinput
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <linux/input-event-codes.h>
#include "starlight.h"

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface iface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

int starlight_input_init(struct starlight_input *input) {
    memset(input, 0, sizeof(*input));

    input->udev = udev_new();
    if (!input->udev) {
        fprintf(stderr, "[Starlight] Failed to create udev context\n");
        return -1;
    }

    input->li = libinput_udev_create_context(&iface, NULL, input->udev);
    if (!input->li) {
        fprintf(stderr, "[Starlight] Failed to create libinput context\n");
        return -1;
    }

    if (libinput_udev_assign_seat(input->li, "seat0") < 0) {
        fprintf(stderr, "[Starlight] Failed to assign seat\n");
        return -1;
    }

    /* Center cursor initially */
    input->cursor_x = 0;
    input->cursor_y = 0;

    printf("[Starlight] Input initialized\n");
    return 0;
}

int starlight_input_process(struct starlight_server *server) {
    struct starlight_input *input = &server->input;
    struct starlight_display *display = &server->display;

    libinput_dispatch(input->li);

    struct libinput_event *event;
    while ((event = libinput_get_event(input->li)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);

        switch (type) {
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer *p =
                libinput_event_get_pointer_event(event);
            double dx = libinput_event_pointer_get_dx(p);
            double dy = libinput_event_pointer_get_dy(p);

            input->cursor_x += dx;
            input->cursor_y += dy;

            /* Clamp to screen bounds */
            if (input->cursor_x < 0) input->cursor_x = 0;
            if (input->cursor_y < 0) input->cursor_y = 0;
            if (input->cursor_x >= display->mode.hdisplay)
                input->cursor_x = display->mode.hdisplay - 1;
            if (input->cursor_y >= display->mode.vdisplay)
                input->cursor_y = display->mode.vdisplay - 1;
            break;
        }

        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            struct libinput_event_pointer *p =
                libinput_event_get_pointer_event(event);
            input->cursor_x = libinput_event_pointer_get_absolute_x_transformed(
                p, display->mode.hdisplay);
            input->cursor_y = libinput_event_pointer_get_absolute_y_transformed(
                p, display->mode.vdisplay);
            break;
        }

        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *k =
                libinput_event_get_keyboard_event(event);
            uint32_t key = libinput_event_keyboard_get_key(k);
            enum libinput_key_state state =
                libinput_event_keyboard_get_key_state(k);

            if (key == KEY_ESC && state == LIBINPUT_KEY_STATE_PRESSED) {
                printf("[Starlight] Escape pressed — shutting down\n");
                server->running = 0;
            }
            break;
        }

        default:
            break;
        }

        libinput_event_destroy(event);
    }

    return 0;
}

void starlight_input_destroy(struct starlight_input *input) {
    if (input->li) libinput_unref(input->li);
    if (input->udev) udev_unref(input->udev);
}
