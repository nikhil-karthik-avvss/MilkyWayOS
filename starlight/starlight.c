/*
 * Starlight Display Server — MilkyWayOS
 * Phase 1: DRM/KMS framebuffer initialization
 *
 * This takes over the screen via DRM and fills it
 * with a deep space blue color.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct starlight_framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint32_t fb_id;
    uint8_t *map;
};

/* Open the DRM device */
static int starlight_open_drm(void) {
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "[Starlight] Failed to open /dev/dri/card0: %s\n",
                strerror(errno));
        return -1;
    }

    /* Check that we can become DRM master */
    if (drmSetMaster(fd) < 0) {
        fprintf(stderr, "[Starlight] Warning: Could not become DRM master: %s\n",
                strerror(errno));
        /* Continue anyway — might work in some setups */
    }

    printf("[Starlight] DRM device opened successfully (fd=%d)\n", fd);
    return fd;
}

/* Find the first connected output and its preferred mode */
static int starlight_find_output(int fd, uint32_t *conn_id,
                                  drmModeModeInfo *mode, uint32_t *crtc_id) {
    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        fprintf(stderr, "[Starlight] Failed to get DRM resources\n");
        return -1;
    }

    /* Loop through connectors to find one that's connected */
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) continue;

        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            /* Use the first (preferred) mode */
            *conn_id = conn->connector_id;
            memcpy(mode, &conn->modes[0], sizeof(drmModeModeInfo));

            /* Find a CRTC for this connector */
            drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                *crtc_id = enc->crtc_id;
                drmModeFreeEncoder(enc);
            } else {
                /* Just use the first available CRTC */
                *crtc_id = res->crtcs[0];
            }

            printf("[Starlight] Found output: %dx%d @ %dHz (connector %u, crtc %u)\n",
                   mode->hdisplay, mode->vdisplay, mode->vrefresh,
                   *conn_id, *crtc_id);

            drmModeFreeConnector(conn);
            drmModeFreeResources(res);
            return 0;
        }
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
    fprintf(stderr, "[Starlight] No connected output found\n");
    return -1;
}

/* Create a dumb framebuffer we can draw to */
static int starlight_create_fb(int fd, struct starlight_framebuffer *fb,
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

    /* Add framebuffer object */
    if (drmModeAddFB(fd, width, height, 24, 32, fb->stride, fb->handle,
                     &fb->fb_id) < 0) {
        fprintf(stderr, "[Starlight] Failed to add framebuffer: %s\n",
                strerror(errno));
        return -1;
    }

    /* Memory-map the buffer so we can draw to it */
    struct drm_mode_map_dumb map = { .handle = fb->handle };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        fprintf(stderr, "[Starlight] Failed to map dumb buffer: %s\n",
                strerror(errno));
        return -1;
    }

    fb->map = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   fd, map.offset);
    if (fb->map == MAP_FAILED) {
        fprintf(stderr, "[Starlight] mmap failed: %s\n", strerror(errno));
        return -1;
    }

    printf("[Starlight] Framebuffer created: %ux%u, stride=%u, size=%u\n",
           fb->width, fb->height, fb->stride, fb->size);
    return 0;
}

/* Fill the screen with a color (XRGB8888 format) */
static void starlight_fill_screen(struct starlight_framebuffer *fb,
                                   uint8_t r, uint8_t g, uint8_t b) {
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            uint32_t offset = y * fb->stride + x * 4;
            fb->map[offset + 0] = b;     /* Blue */
            fb->map[offset + 1] = g;     /* Green */
            fb->map[offset + 2] = r;     /* Red */
            fb->map[offset + 3] = 0xFF;  /* Alpha (unused) */
        }
    }
}

/* Clean up framebuffer */
static void starlight_destroy_fb(int fd, struct starlight_framebuffer *fb) {
    munmap(fb->map, fb->size);
    drmModeRmFB(fd, fb->fb_id);
    struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

int main(void) {
    printf("=== Starlight Display Server v0.1 ===\n");
    printf("=== MilkyWayOS ===\n\n");

    /* Open DRM device */
    int fd = starlight_open_drm();
    if (fd < 0) return 1;

    /* Find a connected display */
    uint32_t conn_id, crtc_id;
    drmModeModeInfo mode;
    if (starlight_find_output(fd, &conn_id, &mode, &crtc_id) < 0) {
        close(fd);
        return 1;
    }

    /* Save the current display state so we can restore it */
    drmModeCrtc *saved_crtc = drmModeGetCrtc(fd, crtc_id);

    /* Create framebuffer */
    struct starlight_framebuffer fb;
    if (starlight_create_fb(fd, &fb, mode.hdisplay, mode.vdisplay) < 0) {
        close(fd);
        return 1;
    }

    /* Fill with deep space blue */
    starlight_fill_screen(&fb, 10, 10, 40);

    /* Set this framebuffer on the display */
    if (drmModeSetCrtc(fd, crtc_id, fb.fb_id, 0, 0, &conn_id, 1, &mode) < 0) {
        fprintf(stderr, "[Starlight] Failed to set CRTC: %s\n", strerror(errno));
        starlight_destroy_fb(fd, &fb);
        close(fd);
        return 1;
    }

    printf("[Starlight] Screen active! Showing deep space blue.\n");
    printf("[Starlight] Press Enter to exit...\n");
    getchar();

    /* Restore previous display state */
    if (saved_crtc) {
        drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                       saved_crtc->x, saved_crtc->y,
                       &conn_id, 1, &saved_crtc->mode);
        drmModeFreeCrtc(saved_crtc);
    }

    /* Cleanup */
    starlight_destroy_fb(fd, &fb);
    drmDropMaster(fd);
    close(fd);

    printf("[Starlight] Shut down cleanly.\n");
    return 0;
}
