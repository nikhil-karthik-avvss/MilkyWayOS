/*
 * Starlight — Window management module
 */

#include <stdio.h>
#include <string.h>
#include "starlight.h"

int starlight_window_create(struct starlight_server *server,
                            int x, int y, int w, int h,
                            const char *title,
                            uint8_t bg_r, uint8_t bg_g, uint8_t bg_b) {
    if (server->window_count >= MAX_WINDOWS) {
        fprintf(stderr, "[Starlight] Maximum windows reached\n");
        return -1;
    }

    int idx = server->window_count;
    struct starlight_window *win = &server->windows[idx];

    win->id = idx;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->bg_r = bg_r;
    win->bg_g = bg_g;
    win->bg_b = bg_b;
    win->visible = 1;
    win->alive = 1;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';

    server->window_order[idx] = idx;
    server->window_count++;
    server->focus = idx;

    printf("[Starlight] Window created: '%s' at (%d,%d) %dx%d\n",
           title, x, y, w, h);
    return idx;
}

void starlight_window_close(struct starlight_server *server, int id) {
    if (id < 0 || id >= server->window_count) return;

    server->windows[id].alive = 0;
    server->windows[id].visible = 0;

    /* Update focus to next visible window */
    server->focus = -1;
    for (int i = server->window_count - 1; i >= 0; i--) {
        int idx = server->window_order[i];
        if (server->windows[idx].alive) {
            server->focus = idx;
            break;
        }
    }

    printf("[Starlight] Window closed: '%s'\n", server->windows[id].title);
}

/* Find topmost window at screen coordinates (checks in reverse draw order) */
int starlight_window_at(struct starlight_server *server, int x, int y) {
    for (int i = server->window_count - 1; i >= 0; i--) {
        int idx = server->window_order[i];
        struct starlight_window *win = &server->windows[idx];
        if (!win->alive || !win->visible) continue;

        /* Check including titlebar */
        int wx = win->x - WINDOW_BORDER;
        int wy = win->y - TITLEBAR_HEIGHT - WINDOW_BORDER;
        int ww = win->width + WINDOW_BORDER * 2;
        int wh = win->height + TITLEBAR_HEIGHT + WINDOW_BORDER * 2;

        if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
            return idx;
        }
    }
    return -1;
}

/* Raise window to top of draw order */
void starlight_window_raise(struct starlight_server *server, int id) {
    /* Find the window in the order list */
    int pos = -1;
    for (int i = 0; i < server->window_count; i++) {
        if (server->window_order[i] == id) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return;

    /* Shift everything after it down one */
    for (int i = pos; i < server->window_count - 1; i++) {
        server->window_order[i] = server->window_order[i + 1];
    }
    /* Put it on top */
    server->window_order[server->window_count - 1] = id;
    server->focus = id;
}

int starlight_window_titlebar_hit(struct starlight_window *win, int x, int y) {
    return (x >= win->x && x < win->x + win->width &&
            y >= win->y - TITLEBAR_HEIGHT && y < win->y);
}

int starlight_window_close_btn_hit(struct starlight_window *win, int x, int y) {
    int btn_x = win->x + win->width - 20;
    int btn_y = win->y - TITLEBAR_HEIGHT + 4;
    return (x >= btn_x && x < btn_x + 16 && y >= btn_y && y < btn_y + 20);
}
