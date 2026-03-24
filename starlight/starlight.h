/*
 * Starlight Display Server v0.7 — MilkyWayOS
 * Core header — with Nova file manager
 */

#ifndef STARLIGHT_H
#define STARLIGHT_H

#include <stdint.h>
#include <time.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libinput.h>
#include <libudev.h>

/* Deep space theme colors */
#define STARLIGHT_BG_R 10
#define STARLIGHT_BG_G 10
#define STARLIGHT_BG_B 40

#define STARLIGHT_CURSOR_R 200
#define STARLIGHT_CURSOR_G 200
#define STARLIGHT_CURSOR_B 255

/* Window theme */
#define TITLEBAR_HEIGHT 28
#define TITLEBAR_R 40
#define TITLEBAR_G 30
#define TITLEBAR_B 80

#define TITLEBAR_ACTIVE_R 80
#define TITLEBAR_ACTIVE_G 50
#define TITLEBAR_ACTIVE_B 160

#define WINDOW_BORDER 2
#define BORDER_R 60
#define BORDER_G 50
#define BORDER_B 120

#define CLOSE_BTN_R 180
#define CLOSE_BTN_G 50
#define CLOSE_BTN_B 50

/* Taskbar theme */
#define TASKBAR_HEIGHT 36
#define TASKBAR_R 18
#define TASKBAR_G 14
#define TASKBAR_B 36

#define TASKBAR_BTN_R 40
#define TASKBAR_BTN_G 30
#define TASKBAR_BTN_B 70

#define TASKBAR_BTN_ACTIVE_R 80
#define TASKBAR_BTN_ACTIVE_G 50
#define TASKBAR_BTN_ACTIVE_B 160

#define TASKBAR_BTN_WIDTH 140
#define TASKBAR_BTN_HEIGHT 26
#define TASKBAR_BTN_MARGIN 4
#define TASKBAR_START_X 120

/* Pulsar terminal settings */
#define PULSAR_FONT_W 6
#define PULSAR_FONT_H 9
#define PULSAR_MAX_COLS 128
#define PULSAR_MAX_ROWS 64
#define PULSAR_SCROLLBACK 256
#define PULSAR_TAB_WIDTH 8

#define PULSAR_FG_R 180
#define PULSAR_FG_G 200
#define PULSAR_FG_B 255

#define PULSAR_CURSOR_R 140
#define PULSAR_CURSOR_G 120
#define PULSAR_CURSOR_B 255

/* Nebula desktop settings */
#define NEBULA_MAX_STARS 300
#define NEBULA_MENU_WIDTH 200
#define NEBULA_MENU_ITEM_HEIGHT 28
#define NEBULA_MENU_R 25
#define NEBULA_MENU_G 20
#define NEBULA_MENU_B 50
#define NEBULA_MENU_HOVER_R 60
#define NEBULA_MENU_HOVER_G 40
#define NEBULA_MENU_HOVER_B 120
#define NEBULA_MENU_BORDER_R 80
#define NEBULA_MENU_BORDER_G 60
#define NEBULA_MENU_BORDER_B 140

/* Nova file manager settings */
#define NOVA_MAX_ENTRIES 256
#define NOVA_ENTRY_HEIGHT 22
#define NOVA_PATHBAR_HEIGHT 26
#define NOVA_TOOLBAR_HEIGHT 28
#define NOVA_ICON_WIDTH 16
#define NOVA_NAME_COL_X 24
#define NOVA_SIZE_COL_X 280
#define NOVA_SCROLL_AMOUNT 3

/* Nova colors */
#define NOVA_BG_R 12
#define NOVA_BG_G 12
#define NOVA_BG_B 28

#define NOVA_PATHBAR_R 20
#define NOVA_PATHBAR_G 16
#define NOVA_PATHBAR_B 40

#define NOVA_TOOLBAR_R 25
#define NOVA_TOOLBAR_G 20
#define NOVA_TOOLBAR_B 50

#define NOVA_ENTRY_HOVER_R 40
#define NOVA_ENTRY_HOVER_G 30
#define NOVA_ENTRY_HOVER_B 80

#define NOVA_ENTRY_SELECTED_R 60
#define NOVA_ENTRY_SELECTED_G 40
#define NOVA_ENTRY_SELECTED_B 120

#define NOVA_DIR_R 120
#define NOVA_DIR_G 160
#define NOVA_DIR_B 255

#define NOVA_FILE_R 170
#define NOVA_FILE_G 180
#define NOVA_FILE_B 200

#define NOVA_LINK_R 140
#define NOVA_LINK_G 220
#define NOVA_LINK_B 180

#define CURSOR_SIZE 16
#define MAX_WINDOWS 16

/* Forward declarations */
struct starlight_server;

struct starlight_framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint32_t fb_id;
    uint8_t *map;
};

/* Terminal cell */
struct pulsar_cell {
    char ch;
    uint8_t fg_r, fg_g, fg_b;
    uint8_t bg_r, bg_g, bg_b;
    int dirty;
};

/* Pulsar terminal state */
struct pulsar_terminal {
    int master_fd;
    pid_t child_pid;
    int cols, rows;
    int cursor_col, cursor_row;
    int scroll_top;
    struct pulsar_cell grid[PULSAR_SCROLLBACK][PULSAR_MAX_COLS];
    int total_rows;
    int active;

    int esc_state;
    char esc_buf[64];
    int esc_len;
};

/* Nova file entry */
struct nova_entry {
    char name[256];
    int is_dir;
    int is_link;
    off_t size;
    mode_t mode;
};

/* Nova file manager state */
struct nova_filemanager {
    char current_path[1024];
    struct nova_entry entries[NOVA_MAX_ENTRIES];
    int entry_count;
    int scroll_offset;      /* First visible entry index */
    int selected;           /* Selected entry index, -1 for none */
    int hover;              /* Hovered entry, -1 for none */
    int visible_rows;       /* How many entries fit in the window */
    int active;
};

struct starlight_window {
    int id;
    int x, y;
    int width, height;
    char title[64];
    uint8_t bg_r, bg_g, bg_b;
    int visible;
    int alive;

    struct pulsar_terminal *terminal;
    struct nova_filemanager *filemanager;
};

struct starlight_display {
    int drm_fd;
    uint32_t conn_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;

    struct starlight_framebuffer fb[2];
    int front;
};

struct starlight_input {
    struct libinput *li;
    struct udev *udev;
    double cursor_x;
    double cursor_y;
    int left_pressed;
    int right_pressed;
    int dragging;
    int drag_window;
    int drag_offset_x;
    int drag_offset_y;
};

/* Nebula star */
struct nebula_star {
    int x, y;
    uint8_t brightness;
    uint8_t size;
};

/* Nebula menu item */
#define NEBULA_MAX_MENU_ITEMS 8

struct nebula_menu_item {
    char label[32];
    int action;
};

struct nebula_menu {
    int visible;
    int x, y;
    struct nebula_menu_item items[NEBULA_MAX_MENU_ITEMS];
    int item_count;
    int hover_index;
};

/* Menu action IDs */
#define NEBULA_ACTION_NEW_TERMINAL  1
#define NEBULA_ACTION_ABOUT         2
#define NEBULA_ACTION_CLOSE_MENU    3
#define NEBULA_ACTION_FILE_MANAGER  4

struct nebula_desktop {
    struct nebula_star stars[NEBULA_MAX_STARS];
    int star_count;

    struct nebula_menu desktop_menu;
    struct nebula_menu launcher_menu;

    int about_visible;
};

struct starlight_server {
    struct starlight_display display;
    struct starlight_input input;
    int running;

    struct starlight_window windows[MAX_WINDOWS];
    int window_count;
    int focus;
    int window_order[MAX_WINDOWS];

    struct nebula_desktop desktop;
};

/* Display functions */
int starlight_display_init(struct starlight_display *display);
void starlight_display_destroy(struct starlight_display *display);
void starlight_display_swap(struct starlight_display *display);
struct starlight_framebuffer *starlight_display_back_buffer(
    struct starlight_display *display);

/* Drawing functions */
void starlight_draw_clear(struct starlight_framebuffer *fb,
                          uint8_t r, uint8_t g, uint8_t b);
void starlight_draw_rect(struct starlight_framebuffer *fb,
                         int x, int y, int w, int h,
                         uint8_t r, uint8_t g, uint8_t b);
void starlight_draw_cursor(struct starlight_framebuffer *fb,
                           int cx, int cy);
void starlight_draw_window(struct starlight_framebuffer *fb,
                           struct starlight_window *win, int focused);
void starlight_draw_text_simple(struct starlight_framebuffer *fb,
                                int x, int y, const char *text,
                                uint8_t r, uint8_t g, uint8_t b);
void starlight_draw_char(struct starlight_framebuffer *fb,
                         int x, int y, char ch,
                         uint8_t r, uint8_t g, uint8_t b);

/* Input functions */
int starlight_input_init(struct starlight_input *input);
void starlight_input_destroy(struct starlight_input *input);
int starlight_input_process(struct starlight_server *server);

/* Window functions */
int starlight_window_create(struct starlight_server *server,
                            int x, int y, int w, int h,
                            const char *title,
                            uint8_t bg_r, uint8_t bg_g, uint8_t bg_b);
void starlight_window_close(struct starlight_server *server, int id);
int starlight_window_at(struct starlight_server *server, int x, int y);
void starlight_window_raise(struct starlight_server *server, int id);
int starlight_window_titlebar_hit(struct starlight_window *win, int x, int y);
int starlight_window_close_btn_hit(struct starlight_window *win, int x, int y);

/* Taskbar functions */
void starlight_draw_taskbar(struct starlight_framebuffer *fb,
                            struct starlight_server *server);
int starlight_taskbar_hit(struct starlight_server *server, int x, int y);
int starlight_taskbar_window_at(struct starlight_server *server, int x, int y);
int starlight_taskbar_launcher_hit(struct starlight_server *server, int x, int y);

/* Pulsar terminal functions */
int pulsar_init(struct pulsar_terminal *term, int cols, int rows);
void pulsar_destroy(struct pulsar_terminal *term);
void pulsar_process_output(struct pulsar_terminal *term);
void pulsar_send_key(struct pulsar_terminal *term, uint32_t key, int shift);
void pulsar_draw(struct starlight_framebuffer *fb,
                 struct starlight_window *win);
int pulsar_create_window(struct starlight_server *server,
                         int x, int y, int w, int h);

/* Nebula desktop functions */
void nebula_init(struct nebula_desktop *desktop, int screen_w, int screen_h);
void nebula_draw_wallpaper(struct starlight_framebuffer *fb,
                           struct nebula_desktop *desktop);
void nebula_draw_menu(struct starlight_framebuffer *fb,
                      struct nebula_menu *menu, int cursor_x, int cursor_y);
void nebula_open_desktop_menu(struct nebula_desktop *desktop, int x, int y);
void nebula_open_launcher_menu(struct nebula_desktop *desktop);
void nebula_close_menus(struct nebula_desktop *desktop);
int nebula_menu_hit(struct nebula_menu *menu, int x, int y);
int nebula_menu_item_at(struct nebula_menu *menu, int x, int y);
int nebula_handle_menu_click(struct starlight_server *server, int x, int y);
void nebula_draw_about(struct starlight_framebuffer *fb,
                       struct starlight_server *server);

/* Nova file manager functions */
int nova_init(struct nova_filemanager *fm, const char *path);
void nova_destroy(struct nova_filemanager *fm);
void nova_navigate(struct nova_filemanager *fm, const char *path);
void nova_navigate_up(struct nova_filemanager *fm);
void nova_enter_selected(struct starlight_server *server, int win_id);
void nova_scroll(struct nova_filemanager *fm, int delta);
void nova_draw(struct starlight_framebuffer *fb, struct starlight_window *win,
               int cursor_x, int cursor_y);
int nova_create_window(struct starlight_server *server,
                       int x, int y, int w, int h, const char *path);
int nova_handle_click(struct starlight_server *server, int win_id,
                      int local_x, int local_y);

#endif
