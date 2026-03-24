/*
 * Starlight — Input module
 * Mouse, keyboard, menus, terminal and file manager input
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

static int shift_held = 0;
static int ctrl_held = 0;

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

    input->drag_window = -1;
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

            if (input->cursor_x < 0) input->cursor_x = 0;
            if (input->cursor_y < 0) input->cursor_y = 0;
            if (input->cursor_x >= display->mode.hdisplay)
                input->cursor_x = display->mode.hdisplay - 1;
            if (input->cursor_y >= display->mode.vdisplay)
                input->cursor_y = display->mode.vdisplay - 1;

            if (input->dragging && input->drag_window >= 0) {
                struct starlight_window *win =
                    &server->windows[input->drag_window];
                win->x = (int)input->cursor_x - input->drag_offset_x;
                win->y = (int)input->cursor_y - input->drag_offset_y;
            }
            break;
        }

        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            struct libinput_event_pointer *p =
                libinput_event_get_pointer_event(event);
            input->cursor_x = libinput_event_pointer_get_absolute_x_transformed(
                p, display->mode.hdisplay);
            input->cursor_y = libinput_event_pointer_get_absolute_y_transformed(
                p, display->mode.vdisplay);

            if (input->dragging && input->drag_window >= 0) {
                struct starlight_window *win =
                    &server->windows[input->drag_window];
                win->x = (int)input->cursor_x - input->drag_offset_x;
                win->y = (int)input->cursor_y - input->drag_offset_y;
            }
            break;
        }

        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer *p =
                libinput_event_get_pointer_event(event);
            uint32_t button = libinput_event_pointer_get_button(p);
            enum libinput_button_state state =
                libinput_event_pointer_get_button_state(p);

            int mx = (int)input->cursor_x;
            int my = (int)input->cursor_y;

            if (button == BTN_RIGHT) {
                if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
                    input->right_pressed = 1;
                    int hit = starlight_window_at(server, mx, my);
                    if (hit < 0 && !starlight_taskbar_hit(server, mx, my)) {
                        nebula_open_desktop_menu(&server->desktop, mx, my);
                    } else {
                        nebula_close_menus(&server->desktop);
                    }
                } else {
                    input->right_pressed = 0;
                }
                break;
            }

            if (button == BTN_LEFT) {
                if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
                    input->left_pressed = 1;

                    /* Menus first */
                    if (server->desktop.desktop_menu.visible ||
                        server->desktop.launcher_menu.visible) {
                        nebula_handle_menu_click(server, mx, my);
                        break;
                    }

                    /* Taskbar */
                    if (starlight_taskbar_hit(server, mx, my)) {
                        if (starlight_taskbar_launcher_hit(server, mx, my)) {
                            if (server->desktop.launcher_menu.visible) {
                                nebula_close_menus(&server->desktop);
                            } else {
                                int taskbar_y = display->mode.vdisplay - TASKBAR_HEIGHT;
                                server->desktop.launcher_menu.y =
                                    taskbar_y - (server->desktop.launcher_menu.item_count *
                                    NEBULA_MENU_ITEM_HEIGHT + 4);
                                nebula_open_launcher_menu(&server->desktop);
                            }
                        } else {
                            int tb_win = starlight_taskbar_window_at(server, mx, my);
                            if (tb_win >= 0) {
                                starlight_window_raise(server, tb_win);
                            }
                        }
                    } else {
                        int hit = starlight_window_at(server, mx, my);
                        if (hit >= 0) {
                            starlight_window_raise(server, hit);
                            struct starlight_window *win =
                                &server->windows[hit];

                            if (starlight_window_close_btn_hit(win, mx, my)) {
                                if (win->terminal) {
                                    pulsar_destroy(win->terminal);
                                    free(win->terminal);
                                    win->terminal = NULL;
                                }
                                if (win->filemanager) {
                                    nova_destroy(win->filemanager);
                                    free(win->filemanager);
                                    win->filemanager = NULL;
                                }
                                starlight_window_close(server, hit);
                            } else if (starlight_window_titlebar_hit(win, mx, my)) {
                                input->dragging = 1;
                                input->drag_window = hit;
                                input->drag_offset_x = mx - win->x;
                                input->drag_offset_y = my - win->y;
                            } else if (win->filemanager) {
                                /* Click inside file manager content */
                                int local_x = mx - win->x;
                                int local_y = my - win->y;
                                nova_handle_click(server, hit, local_x, local_y);
                            }
                        }
                    }
                } else {
                    input->left_pressed = 0;
                    input->dragging = 0;
                    input->drag_window = -1;
                }
            }
            break;
        }

        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *k =
                libinput_event_get_keyboard_event(event);
            uint32_t key = libinput_event_keyboard_get_key(k);
            enum libinput_key_state state =
                libinput_event_keyboard_get_key_state(k);

            if (key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT)
                shift_held = (state == LIBINPUT_KEY_STATE_PRESSED);
            if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL)
                ctrl_held = (state == LIBINPUT_KEY_STATE_PRESSED);

            if (state == LIBINPUT_KEY_STATE_PRESSED) {
                if (key == KEY_ESC) {
                    if (server->desktop.desktop_menu.visible ||
                        server->desktop.launcher_menu.visible) {
                        nebula_close_menus(&server->desktop);
                        break;
                    }
                }

                if (ctrl_held && shift_held && key == KEY_Q) {
                    printf("[Starlight] Ctrl+Shift+Q — shutting down\n");
                    server->running = 0;
                    break;
                }

                if (ctrl_held && shift_held && key == KEY_T) {
                    static int term_x = 80, term_y = 80;
                    pulsar_create_window(server, term_x, term_y, 500, 350);
                    term_x += 30; term_y += 30;
                    if (term_x > 400) { term_x = 80; term_y = 80; }
                    break;
                }

                if (ctrl_held && shift_held && key == KEY_F) {
                    static int fm_x = 100, fm_y = 80;
                    nova_create_window(server, fm_x, fm_y, 450, 400, "/");
                    fm_x += 30; fm_y += 30;
                    if (fm_x > 350) { fm_x = 100; fm_y = 80; }
                    break;
                }

                /* Forward to terminal */
                if (server->focus >= 0) {
                    struct starlight_window *win =
                        &server->windows[server->focus];
                    if (win->terminal && win->terminal->active) {
                        if (ctrl_held) {
                            char ctrl_ch = 0;
                            if (key == KEY_C) ctrl_ch = 3;
                            else if (key == KEY_D) ctrl_ch = 4;
                            else if (key == KEY_Z) ctrl_ch = 26;
                            else if (key == KEY_L) ctrl_ch = 12;
                            if (ctrl_ch) {
                                write(win->terminal->master_fd, &ctrl_ch, 1);
                                break;
                            }
                        }
                        pulsar_send_key(win->terminal, key, shift_held);
                    }

                    /* Scroll file manager with arrow keys */
                    if (win->filemanager && win->filemanager->active) {
                        if (key == KEY_UP && win->filemanager->selected > 0) {
                            win->filemanager->selected--;
                            if (win->filemanager->selected < win->filemanager->scroll_offset)
                                win->filemanager->scroll_offset = win->filemanager->selected;
                        }
                        if (key == KEY_DOWN && win->filemanager->selected < win->filemanager->entry_count - 1) {
                            win->filemanager->selected++;
                            if (win->filemanager->selected >= win->filemanager->scroll_offset + win->filemanager->visible_rows)
                                win->filemanager->scroll_offset = win->filemanager->selected - win->filemanager->visible_rows + 1;
                        }
                        if (key == KEY_ENTER && win->filemanager->selected >= 0) {
                            nova_enter_selected(server, server->focus);
                        }
                        if (key == KEY_BACKSPACE) {
                            nova_navigate_up(win->filemanager);
                            snprintf(win->title, sizeof(win->title), "Nova - %s",
                                     win->filemanager->current_path);
                        }
                    }
                }
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
