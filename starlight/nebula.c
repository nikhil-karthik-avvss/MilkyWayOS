/*
 * Nebula — Desktop Environment for MilkyWayOS
 * Wallpaper with starfield, right-click menus, app launcher, about dialog
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "starlight.h"

static uint32_t nebula_rand_state = 0;
static uint32_t nebula_rand(void) {
    nebula_rand_state ^= nebula_rand_state << 13;
    nebula_rand_state ^= nebula_rand_state >> 17;
    nebula_rand_state ^= nebula_rand_state << 5;
    return nebula_rand_state;
}

void nebula_init(struct nebula_desktop *desktop, int screen_w, int screen_h) {
    memset(desktop, 0, sizeof(*desktop));

    nebula_rand_state = (uint32_t)time(NULL) ^ 0xDEADBEEF;

    desktop->star_count = NEBULA_MAX_STARS;
    for (int i = 0; i < desktop->star_count; i++) {
        desktop->stars[i].x = nebula_rand() % screen_w;
        desktop->stars[i].y = nebula_rand() % (screen_h - TASKBAR_HEIGHT);
        desktop->stars[i].brightness = 40 + (nebula_rand() % 200);
        desktop->stars[i].size = nebula_rand() % 3;
    }

    /* Desktop right-click menu */
    desktop->desktop_menu.visible = 0;
    desktop->desktop_menu.hover_index = -1;
    desktop->desktop_menu.item_count = 4;

    strcpy(desktop->desktop_menu.items[0].label, "New Terminal");
    desktop->desktop_menu.items[0].action = NEBULA_ACTION_NEW_TERMINAL;

    strcpy(desktop->desktop_menu.items[1].label, "File Manager");
    desktop->desktop_menu.items[1].action = NEBULA_ACTION_FILE_MANAGER;

    strcpy(desktop->desktop_menu.items[2].label, "About MilkyWayOS");
    desktop->desktop_menu.items[2].action = NEBULA_ACTION_ABOUT;

    strcpy(desktop->desktop_menu.items[3].label, "Close Menu");
    desktop->desktop_menu.items[3].action = NEBULA_ACTION_CLOSE_MENU;

    /* Launcher menu */
    desktop->launcher_menu.visible = 0;
    desktop->launcher_menu.hover_index = -1;
    desktop->launcher_menu.item_count = 4;

    strcpy(desktop->launcher_menu.items[0].label, "Pulsar Terminal");
    desktop->launcher_menu.items[0].action = NEBULA_ACTION_NEW_TERMINAL;

    strcpy(desktop->launcher_menu.items[1].label, "Nova File Manager");
    desktop->launcher_menu.items[1].action = NEBULA_ACTION_FILE_MANAGER;

    strcpy(desktop->launcher_menu.items[2].label, "About MilkyWayOS");
    desktop->launcher_menu.items[2].action = NEBULA_ACTION_ABOUT;

    strcpy(desktop->launcher_menu.items[3].label, "Close Menu");
    desktop->launcher_menu.items[3].action = NEBULA_ACTION_CLOSE_MENU;

    desktop->about_visible = 0;

    printf("[Nebula] Desktop initialized with %d stars\n", desktop->star_count);
}

void nebula_draw_wallpaper(struct starlight_framebuffer *fb,
                           struct nebula_desktop *desktop) {
    int cx = fb->width / 2;
    int cy = (fb->height - TASKBAR_HEIGHT) / 2;
    for (int y = 0; y < (int)fb->height - TASKBAR_HEIGHT; y++) {
        for (int x = 0; x < (int)fb->width; x++) {
            int dx = x - cx;
            int dy = y - cy;
            int dist_sq = dx * dx + dy * dy;
            int max_dist = cx * cx + cy * cy;

            int glow = 0;
            if (dist_sq < max_dist / 4) {
                glow = 8 - (dist_sq * 8 / (max_dist / 4));
                if (glow < 0) glow = 0;
            }

            if (glow > 0) {
                uint32_t off = y * fb->stride + x * 4;
                int b = fb->map[off + 0] + glow;
                int g = fb->map[off + 1] + glow / 2;
                int r = fb->map[off + 2] + glow;
                if (b > 255) b = 255;
                if (g > 255) g = 255;
                if (r > 255) r = 255;
                fb->map[off + 0] = b;
                fb->map[off + 1] = g;
                fb->map[off + 2] = r;
            }
        }
    }

    for (int i = 0; i < desktop->star_count; i++) {
        struct nebula_star *star = &desktop->stars[i];
        int x = star->x;
        int y = star->y;

        if (x < 0 || x >= (int)fb->width || y < 0 ||
            y >= (int)fb->height - TASKBAR_HEIGHT)
            continue;

        uint8_t br = star->brightness;
        uint8_t r = br * 3 / 4;
        uint8_t g = br * 3 / 4;
        uint8_t b = br;

        uint32_t off = y * fb->stride + x * 4;
        fb->map[off + 0] = b;
        fb->map[off + 1] = g;
        fb->map[off + 2] = r;
        fb->map[off + 3] = 0xFF;

        if (star->size >= 1) {
            if (x + 1 < (int)fb->width) {
                off = y * fb->stride + (x + 1) * 4;
                fb->map[off + 0] = b / 2; fb->map[off + 1] = g / 2; fb->map[off + 2] = r / 2;
            }
            if (x - 1 >= 0) {
                off = y * fb->stride + (x - 1) * 4;
                fb->map[off + 0] = b / 2; fb->map[off + 1] = g / 2; fb->map[off + 2] = r / 2;
            }
            if (y + 1 < (int)fb->height) {
                off = (y + 1) * fb->stride + x * 4;
                fb->map[off + 0] = b / 2; fb->map[off + 1] = g / 2; fb->map[off + 2] = r / 2;
            }
            if (y - 1 >= 0) {
                off = (y - 1) * fb->stride + x * 4;
                fb->map[off + 0] = b / 2; fb->map[off + 1] = g / 2; fb->map[off + 2] = r / 2;
            }
        }

        if (star->size >= 2) {
            off = y * fb->stride + x * 4;
            fb->map[off + 0] = 255; fb->map[off + 1] = 255; fb->map[off + 2] = 255;
        }
    }
}

void nebula_draw_menu(struct starlight_framebuffer *fb,
                      struct nebula_menu *menu, int cursor_x, int cursor_y) {
    if (!menu->visible) return;

    int mw = NEBULA_MENU_WIDTH;
    int mh = menu->item_count * NEBULA_MENU_ITEM_HEIGHT + 4;

    starlight_draw_rect(fb, menu->x - 1, menu->y - 1, mw + 2, mh + 2,
                        NEBULA_MENU_BORDER_R, NEBULA_MENU_BORDER_G,
                        NEBULA_MENU_BORDER_B);

    starlight_draw_rect(fb, menu->x, menu->y, mw, mh,
                        NEBULA_MENU_R, NEBULA_MENU_G, NEBULA_MENU_B);

    menu->hover_index = -1;
    if (cursor_x >= menu->x && cursor_x < menu->x + mw &&
        cursor_y >= menu->y && cursor_y < menu->y + mh) {
        menu->hover_index = (cursor_y - menu->y - 2) / NEBULA_MENU_ITEM_HEIGHT;
        if (menu->hover_index >= menu->item_count)
            menu->hover_index = -1;
    }

    for (int i = 0; i < menu->item_count; i++) {
        int iy = menu->y + 2 + i * NEBULA_MENU_ITEM_HEIGHT;

        if (i == menu->hover_index) {
            starlight_draw_rect(fb, menu->x + 2, iy,
                               mw - 4, NEBULA_MENU_ITEM_HEIGHT,
                               NEBULA_MENU_HOVER_R, NEBULA_MENU_HOVER_G,
                               NEBULA_MENU_HOVER_B);
        }

        starlight_draw_text_simple(fb, menu->x + 12, iy + 10,
                                   menu->items[i].label, 200, 200, 230);
    }
}

void nebula_open_desktop_menu(struct nebula_desktop *desktop, int x, int y) {
    desktop->launcher_menu.visible = 0;
    desktop->desktop_menu.visible = 1;
    desktop->desktop_menu.x = x;
    desktop->desktop_menu.y = y;
    desktop->desktop_menu.hover_index = -1;
}

void nebula_open_launcher_menu(struct nebula_desktop *desktop) {
    desktop->desktop_menu.visible = 0;
    desktop->launcher_menu.visible = 1;
    desktop->launcher_menu.x = 4;
    desktop->launcher_menu.hover_index = -1;
}

void nebula_close_menus(struct nebula_desktop *desktop) {
    desktop->desktop_menu.visible = 0;
    desktop->launcher_menu.visible = 0;
}

int nebula_menu_hit(struct nebula_menu *menu, int x, int y) {
    if (!menu->visible) return 0;
    int mw = NEBULA_MENU_WIDTH;
    int mh = menu->item_count * NEBULA_MENU_ITEM_HEIGHT + 4;
    return (x >= menu->x && x < menu->x + mw &&
            y >= menu->y && y < menu->y + mh);
}

int nebula_menu_item_at(struct nebula_menu *menu, int x, int y) {
    if (!menu->visible) return -1;
    int mw = NEBULA_MENU_WIDTH;
    int mh = menu->item_count * NEBULA_MENU_ITEM_HEIGHT + 4;
    if (x < menu->x || x >= menu->x + mw ||
        y < menu->y || y >= menu->y + mh)
        return -1;

    int idx = (y - menu->y - 2) / NEBULA_MENU_ITEM_HEIGHT;
    if (idx >= 0 && idx < menu->item_count) return idx;
    return -1;
}

int nebula_handle_menu_click(struct starlight_server *server, int x, int y) {
    struct nebula_desktop *desktop = &server->desktop;
    struct nebula_menu *menu = NULL;

    if (nebula_menu_hit(&desktop->desktop_menu, x, y)) {
        menu = &desktop->desktop_menu;
    } else if (nebula_menu_hit(&desktop->launcher_menu, x, y)) {
        menu = &desktop->launcher_menu;
    }

    if (!menu) {
        nebula_close_menus(desktop);
        return 0;
    }

    int idx = nebula_menu_item_at(menu, x, y);
    if (idx < 0) return 0;

    int action = menu->items[idx].action;
    nebula_close_menus(desktop);

    switch (action) {
    case NEBULA_ACTION_NEW_TERMINAL: {
        static int term_x = 120;
        static int term_y = 100;
        pulsar_create_window(server, term_x, term_y, 500, 350);
        term_x += 30; term_y += 30;
        if (term_x > 400) { term_x = 120; term_y = 100; }
        return 1;
    }

    case NEBULA_ACTION_FILE_MANAGER: {
        static int fm_x = 100;
        static int fm_y = 80;
        nova_create_window(server, fm_x, fm_y, 450, 400, "/");
        fm_x += 30; fm_y += 30;
        if (fm_x > 350) { fm_x = 100; fm_y = 80; }
        return 1;
    }

    case NEBULA_ACTION_ABOUT: {
        int sw = server->display.mode.hdisplay;
        int sh = server->display.mode.vdisplay;
        int aw = 360, ah = 200;
        starlight_window_create(server, (sw - aw) / 2, (sh - ah) / 2,
                               aw, ah, "About MilkyWayOS", 15, 12, 35);
        return 1;
    }

    case NEBULA_ACTION_CLOSE_MENU:
        return 1;

    default:
        return 0;
    }
}

void nebula_draw_about(struct starlight_framebuffer *fb,
                       struct starlight_server *server) {
    for (int i = 0; i < server->window_count; i++) {
        struct starlight_window *win = &server->windows[i];
        if (!win->alive || !win->visible) continue;
        if (strcmp(win->title, "About MilkyWayOS") != 0) continue;
        if (win->terminal || win->filemanager) continue;

        int tx = win->x + 20;
        int ty = win->y + 20;

        starlight_draw_text_simple(fb, tx, ty,
            "MilkyWayOS", 180, 150, 255);
        ty += 20;
        starlight_draw_text_simple(fb, tx, ty,
            "A custom Linux distribution", 160, 160, 200);
        ty += 14;
        starlight_draw_text_simple(fb, tx, ty,
            "built from scratch.", 160, 160, 200);
        ty += 24;
        starlight_draw_text_simple(fb, tx, ty,
            "Display Server: Starlight v0.7", 140, 140, 180);
        ty += 14;
        starlight_draw_text_simple(fb, tx, ty,
            "Desktop: Nebula v0.1", 140, 140, 180);
        ty += 14;
        starlight_draw_text_simple(fb, tx, ty,
            "Terminal: Pulsar v0.1", 140, 140, 180);
        ty += 14;
        starlight_draw_text_simple(fb, tx, ty,
            "File Manager: Nova v0.1", 140, 140, 180);
        ty += 14;
        starlight_draw_text_simple(fb, tx, ty,
            "Base: Debian 12 (Bookworm)", 140, 140, 180);
        ty += 24;
        starlight_draw_text_simple(fb, tx, ty,
            "Created by Nikhil", 120, 180, 255);
    }
}
