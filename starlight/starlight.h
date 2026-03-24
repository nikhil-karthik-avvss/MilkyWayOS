/*
 * Starlight Display Server v0.3 — MilkyWayOS
 * Core header — now with window management
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

#define CURSOR_SIZE 16
#define MAX_WINDOWS 16

struct starlight_framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint32_t fb_id;
    uint8_t *map;
};

struct starlight_window {
    int id;
    int x, y;
    int width, height;
    char title[64];
    uint8_t bg_r, bg_g, bg_b;
    int visible;
    int alive;
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
    int focus;  /* Index of focused window, -1 for none */
    int window_order[MAX_WINDOWS];  /* Drawing order (back to front) */
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

#endif
