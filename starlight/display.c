/*
 * Starlight — Display module
 * DRM/KMS initialization and double-buffered page flipping
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "starlight.h"

static int create_fb(int fd, struct starlight_framebuffer *fb,
                     uint32_t width, uint32_t height) {
    struct drm_mode_create_dumb create = {
        .width = width,
        .height = height,
        .bpp = 32,
    };

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        fprintf(stderr, "[Starlight] Failed to create dumb buffer: %s\n",
                strerror(errno));
        return -1;
    }

    fb->width = width;
    fb->height = height;
    fb->stride = create.pitch;
    fb->size = create.size;
    fb->handle = create.handle;

    if (drmModeAddFB(fd, width, height, 24, 32, fb->stride,
                     fb->handle, &fb->fb_id) < 0) {
        fprintf(stderr, "[Starlight] Failed to add framebuffer: %s\n",
                strerror(errno));
        return -1;
    }

    struct drm_mode_map_dumb map = { .handle = fb->handle };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        fprintf(stderr, "[Starlight] Failed to map buffer: %s\n",
                strerror(errno));
        return -1;
    }

    fb->map = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   fd, map.offset);
    if (fb->map == MAP_FAILED) {
        fprintf(stderr, "[Starlight] mmap failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static void destroy_fb(int fd, struct starlight_framebuffer *fb) {
    if (fb->map) munmap(fb->map, fb->size);
    if (fb->fb_id) drmModeRmFB(fd, fb->fb_id);
    struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

int starlight_display_init(struct starlight_display *display) {
    memset(display, 0, sizeof(*display));
    display->front = 0;

    display->drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (display->drm_fd < 0) {
        fprintf(stderr, "[Starlight] Failed to open /dev/dri/card0: %s\n",
                strerror(errno));
        return -1;
    }

    drmSetMaster(display->drm_fd);

    drmModeRes *res = drmModeGetResources(display->drm_fd);
    if (!res) {
        fprintf(stderr, "[Starlight] Failed to get DRM resources\n");
        return -1;
    }

    int found = 0;
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(display->drm_fd,
                                                      res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            display->conn_id = conn->connector_id;
            memcpy(&display->mode, &conn->modes[0], sizeof(drmModeModeInfo));

            drmModeEncoder *enc = drmModeGetEncoder(display->drm_fd,
                                                     conn->encoder_id);
            if (enc) {
                display->crtc_id = enc->crtc_id;
                drmModeFreeEncoder(enc);
            } else {
                display->crtc_id = res->crtcs[0];
            }

            found = 1;
            drmModeFreeConnector(conn);
            break;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);

    if (!found) {
        fprintf(stderr, "[Starlight] No connected display found\n");
        return -1;
    }

    printf("[Starlight] Display: %dx%d @ %dHz\n",
           display->mode.hdisplay, display->mode.vdisplay,
           display->mode.vrefresh);

    display->saved_crtc = drmModeGetCrtc(display->drm_fd, display->crtc_id);

    for (int i = 0; i < 2; i++) {
        if (create_fb(display->drm_fd, &display->fb[i],
                      display->mode.hdisplay, display->mode.vdisplay) < 0) {
            return -1;
        }
    }

    if (drmModeSetCrtc(display->drm_fd, display->crtc_id,
                       display->fb[0].fb_id, 0, 0,
                       &display->conn_id, 1, &display->mode) < 0) {
        fprintf(stderr, "[Starlight] Failed to set CRTC: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

void starlight_display_swap(struct starlight_display *display) {
    int back = 1 - display->front;
    drmModeSetCrtc(display->drm_fd, display->crtc_id,
                   display->fb[back].fb_id, 0, 0,
                   &display->conn_id, 1, &display->mode);
    display->front = back;
}

struct starlight_framebuffer *starlight_display_back_buffer(
        struct starlight_display *display) {
    return &display->fb[1 - display->front];
}

void starlight_display_destroy(struct starlight_display *display) {
    if (display->saved_crtc) {
        drmModeSetCrtc(display->drm_fd, display->saved_crtc->crtc_id,
                       display->saved_crtc->buffer_id,
                       display->saved_crtc->x, display->saved_crtc->y,
                       &display->conn_id, 1, &display->saved_crtc->mode);
        drmModeFreeCrtc(display->saved_crtc);
    }

    for (int i = 0; i < 2; i++) {
        destroy_fb(display->drm_fd, &display->fb[i]);
    }

    drmDropMaster(display->drm_fd);
    close(display->drm_fd);
}
