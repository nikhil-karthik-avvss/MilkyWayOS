/*
 * Starlight Display Server v0.5 — MilkyWayOS
 * Core header — now with Pulsar terminal
 */

#ifndef STARLIGHT_H
#define STARLIGHT_H

#include <stdint.h>
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
    int master_fd;          /* PTY master */
    pid_t child_pid;        /* Shell process */
    int cols, rows;         /* Grid dimensions */
    int cursor_col, cursor_row;  /* Cursor position */
    int scroll_top;         /* First visible row in buffer */
    struct pulsar_cell grid[PULSAR_SCROLLBACK][PULSAR_MAX_COLS];
    int total_rows;         /* Total rows written (for scrollback) */
    int active;             /* Is terminal alive? */

    /* ANSI escape sequence parser state */
    int esc_state;          /* 0=normal, 1=got ESC, 2=got CSI */
    char esc_buf[64];
    int esc_len;
};

struct starlight_window {
    int id;
    int x, y;
    int width, height;
    char title[64];
    uint8_t bg_r, bg_g, bg_b;
    int visible;
    int alive;

    /* Terminal (NULL if not a terminal window) */
    struct pulsar_terminal *terminal;
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
    int dragging;
    int drag_window;
    int drag_offset_x;
    int drag_offset_y;
};

struct starlight_server {
    struct starlight_display display;
    struct starlight_input input;
    int running;

    struct starlight_window windows[MAX_WINDOWS];
    int window_count;
    int focus;
    int window_order[MAX_WINDOWS];
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

/* Pulsar terminal functions */
int pulsar_init(struct pulsar_terminal *term, int cols, int rows);
void pulsar_destroy(struct pulsar_terminal *term);
void pulsar_process_output(struct pulsar_terminal *term);
void pulsar_send_key(struct pulsar_terminal *term, uint32_t key, int shift);
void pulsar_draw(struct starlight_framebuffer *fb,
                 struct starlight_window *win);
int pulsar_create_window(struct starlight_server *server,
                         int x, int y, int w, int h);

#endif
