/*
 * System Info Widget — MilkyWayOS
 * Displays CPU, RAM, and uptime on the desktop
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "starlight.h"

static void read_uptime(char *buf, int len) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) { snprintf(buf, len, "??"); return; }

    double secs = 0;
    if (fscanf(f, "%lf", &secs) != 1) secs = 0;
    fclose(f);

    int h = (int)secs / 3600;
    int m = ((int)secs % 3600) / 60;
    int s = (int)secs % 60;

    if (h > 0)
        snprintf(buf, len, "%dh %dm %ds", h, m, s);
    else if (m > 0)
        snprintf(buf, len, "%dm %ds", m, s);
    else
        snprintf(buf, len, "%ds", s);
}

static void read_memory(char *buf, int len) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { snprintf(buf, len, "??"); return; }

    long total_kb = 0, avail_kb = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", &total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", &avail_kb);
    }
    fclose(f);

    long used_kb = total_kb - avail_kb;
    snprintf(buf, len, "%ldM / %ldM",
             used_kb / 1024, total_kb / 1024);
}

static void read_cpu_usage(char *buf, int len) {
    static long prev_total = 0, prev_idle = 0;

    FILE *f = fopen("/proc/stat", "r");
    if (!f) { snprintf(buf, len, "??%%"); return; }

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        snprintf(buf, len, "??%%");
        return;
    }
    fclose(f);

    long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
               &user, &nice, &system, &idle, &iowait,
               &irq, &softirq, &steal) < 4) {
        snprintf(buf, len, "??%%");
        return;
    }

    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long total_diff = total - prev_total;
    long idle_diff = idle - prev_idle;

    int usage = 0;
    if (total_diff > 0)
        usage = (int)(100 * (total_diff - idle_diff) / total_diff);

    prev_total = total;
    prev_idle = idle;

    snprintf(buf, len, "%d%%", usage);
}

void sysinfo_draw(struct starlight_framebuffer *fb, int screen_w) {
    int wx = screen_w - SYSINFO_WIDTH - SYSINFO_MARGIN;
    int wy = SYSINFO_MARGIN;

    /* Semi-transparent background */
    starlight_draw_rect(fb, wx, wy, SYSINFO_WIDTH, SYSINFO_HEIGHT,
                        15, 12, 35);

    /* Border */
    starlight_draw_rect(fb, wx, wy, SYSINFO_WIDTH, 1,
                        BORDER_R, BORDER_G, BORDER_B);
    starlight_draw_rect(fb, wx, wy + SYSINFO_HEIGHT - 1, SYSINFO_WIDTH, 1,
                        BORDER_R, BORDER_G, BORDER_B);
    starlight_draw_rect(fb, wx, wy, 1, SYSINFO_HEIGHT,
                        BORDER_R, BORDER_G, BORDER_B);
    starlight_draw_rect(fb, wx + SYSINFO_WIDTH - 1, wy, 1, SYSINFO_HEIGHT,
                        BORDER_R, BORDER_G, BORDER_B);

    int tx = wx + 10;
    int ty = wy + 10;

    /* Title */
    starlight_draw_text_simple(fb, tx, ty, "System Info",
                               140, 120, 200);
    ty += 16;

    /* Separator */
    starlight_draw_rect(fb, wx + 6, ty, SYSINFO_WIDTH - 12, 1,
                        BORDER_R, BORDER_G, BORDER_B);
    ty += 8;

    /* CPU */
    char cpu_buf[32];
    read_cpu_usage(cpu_buf, sizeof(cpu_buf));
    starlight_draw_text_simple(fb, tx, ty, "CPU:", 100, 100, 150);
    starlight_draw_text_simple(fb, tx + 50, ty, cpu_buf, 180, 200, 255);
    ty += 16;

    /* Memory */
    char mem_buf[48];
    read_memory(mem_buf, sizeof(mem_buf));
    starlight_draw_text_simple(fb, tx, ty, "RAM:", 100, 100, 150);
    starlight_draw_text_simple(fb, tx + 50, ty, mem_buf, 180, 200, 255);
    ty += 16;

    /* Uptime */
    char up_buf[32];
    read_uptime(up_buf, sizeof(up_buf));
    starlight_draw_text_simple(fb, tx, ty, "Up:", 100, 100, 150);
    starlight_draw_text_simple(fb, tx + 50, ty, up_buf, 180, 200, 255);
}
