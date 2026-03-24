/*
 * Nova — File Manager for MilkyWayOS
 * Browse the filesystem in a Starlight window
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "starlight.h"

/* Compare function for sorting entries: dirs first, then alphabetical */
static int nova_entry_compare(const void *a, const void *b) {
    const struct nova_entry *ea = (const struct nova_entry *)a;
    const struct nova_entry *eb = (const struct nova_entry *)b;

    /* ".." always first */
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;

    /* Directories before files */
    if (ea->is_dir && !eb->is_dir) return -1;
    if (!ea->is_dir && eb->is_dir) return 1;

    /* Alphabetical */
    return strcasecmp(ea->name, eb->name);
}

/* Format file size into human-readable string */
static void nova_format_size(off_t size, char *buf, int buf_len) {
    if (size < 1024) {
        snprintf(buf, buf_len, "%ldB", (long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, buf_len, "%.1fK", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buf, buf_len, "%.1fM", size / (1024.0 * 1024.0));
    } else {
        snprintf(buf, buf_len, "%.1fG", size / (1024.0 * 1024.0 * 1024.0));
    }
}

/* Read directory contents */
static void nova_read_dir(struct nova_filemanager *fm) {
    fm->entry_count = 0;
    fm->selected = -1;
    fm->hover = -1;
    fm->scroll_offset = 0;

    DIR *dir = opendir(fm->current_path);
    if (!dir) {
        fprintf(stderr, "[Nova] Failed to open directory: %s\n",
                fm->current_path);
        return;
    }

    /* Always add ".." unless at root */
    if (strcmp(fm->current_path, "/") != 0) {
        struct nova_entry *e = &fm->entries[fm->entry_count];
        strcpy(e->name, "..");
        e->is_dir = 1;
        e->is_link = 0;
        e->size = 0;
        e->mode = 0;
        fm->entry_count++;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && fm->entry_count < NOVA_MAX_ENTRIES) {
        /* Skip . and .. (we already added ..) */
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        /* Skip hidden files */
        if (ent->d_name[0] == '.') continue;

        struct nova_entry *e = &fm->entries[fm->entry_count];
        strncpy(e->name, ent->d_name, sizeof(e->name) - 1);
        e->name[sizeof(e->name) - 1] = '\0';

        /* Get file info */
        char fullpath[1280];
        snprintf(fullpath, sizeof(fullpath), "%s/%s",
                 fm->current_path, ent->d_name);

        struct stat st;
        if (lstat(fullpath, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->is_link = S_ISLNK(st.st_mode);
            e->size = st.st_size;
            e->mode = st.st_mode;
        } else {
            e->is_dir = (ent->d_type == DT_DIR);
            e->is_link = (ent->d_type == DT_LNK);
            e->size = 0;
            e->mode = 0;
        }

        fm->entry_count++;
    }

    closedir(dir);

    /* Sort: dirs first, then alphabetical */
    if (fm->entry_count > 1) {
        int start = (strcmp(fm->entries[0].name, "..") == 0) ? 1 : 0;
        qsort(&fm->entries[start], fm->entry_count - start,
              sizeof(struct nova_entry), nova_entry_compare);
    }

    printf("[Nova] Loaded %d entries from %s\n",
           fm->entry_count, fm->current_path);
}

int nova_init(struct nova_filemanager *fm, const char *path) {
    memset(fm, 0, sizeof(*fm));
    fm->active = 1;
    fm->selected = -1;
    fm->hover = -1;
    strncpy(fm->current_path, path, sizeof(fm->current_path) - 1);

    nova_read_dir(fm);
    return 0;
}

void nova_destroy(struct nova_filemanager *fm) {
    fm->active = 0;
}

void nova_navigate(struct nova_filemanager *fm, const char *path) {
    /* Check if we can access the directory */
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "[Nova] Cannot access: %s\n", path);
        return;
    }
    closedir(dir);

    strncpy(fm->current_path, path, sizeof(fm->current_path) - 1);

    /* Normalize path: remove trailing slash unless root */
    int len = strlen(fm->current_path);
    if (len > 1 && fm->current_path[len - 1] == '/') {
        fm->current_path[len - 1] = '\0';
    }

    nova_read_dir(fm);
}

void nova_navigate_up(struct nova_filemanager *fm) {
    if (strcmp(fm->current_path, "/") == 0) return;

    char *last_slash = strrchr(fm->current_path, '/');
    if (last_slash && last_slash != fm->current_path) {
        *last_slash = '\0';
    } else {
        strcpy(fm->current_path, "/");
    }

    nova_read_dir(fm);
}

void nova_enter_selected(struct starlight_server *server, int win_id) {
    struct starlight_window *win = &server->windows[win_id];
    struct nova_filemanager *fm = win->filemanager;
    if (!fm || fm->selected < 0 || fm->selected >= fm->entry_count) return;

    struct nova_entry *e = &fm->entries[fm->selected];

    if (strcmp(e->name, "..") == 0) {
        nova_navigate_up(fm);
        /* Update window title */
        snprintf(win->title, sizeof(win->title), "Nova - %s", fm->current_path);
        return;
    }

    if (e->is_dir) {
        char newpath[1280];
        if (strcmp(fm->current_path, "/") == 0) {
            snprintf(newpath, sizeof(newpath), "/%s", e->name);
        } else {
            snprintf(newpath, sizeof(newpath), "%s/%s",
                     fm->current_path, e->name);
        }
        nova_navigate(fm, newpath);
        snprintf(win->title, sizeof(win->title), "Nova - %s", fm->current_path);
    }
    /* For regular files, we could open them later. For now, just select. */
}

void nova_scroll(struct nova_filemanager *fm, int delta) {
    fm->scroll_offset += delta;
    if (fm->scroll_offset < 0) fm->scroll_offset = 0;
    int max_scroll = fm->entry_count - fm->visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (fm->scroll_offset > max_scroll) fm->scroll_offset = max_scroll;
}

/* Draw a simple folder icon */
static void nova_draw_folder_icon(struct starlight_framebuffer *fb,
                                   int x, int y) {
    /* Folder tab */
    starlight_draw_rect(fb, x, y + 1, 6, 3,
                        NOVA_DIR_R, NOVA_DIR_G, NOVA_DIR_B);
    /* Folder body */
    starlight_draw_rect(fb, x, y + 3, 12, 8,
                        NOVA_DIR_R, NOVA_DIR_G, NOVA_DIR_B);
    /* Inner shade */
    starlight_draw_rect(fb, x + 1, y + 5, 10, 5,
                        NOVA_DIR_R * 3 / 4, NOVA_DIR_G * 3 / 4,
                        NOVA_DIR_B * 3 / 4);
}

/* Draw a simple file icon */
static void nova_draw_file_icon(struct starlight_framebuffer *fb,
                                 int x, int y) {
    /* File body */
    starlight_draw_rect(fb, x + 1, y, 9, 12,
                        NOVA_FILE_R, NOVA_FILE_G, NOVA_FILE_B);
    /* Dog ear */
    starlight_draw_rect(fb, x + 7, y, 3, 3,
                        NOVA_FILE_R * 3 / 4, NOVA_FILE_G * 3 / 4,
                        NOVA_FILE_B * 3 / 4);
    /* Lines */
    starlight_draw_rect(fb, x + 3, y + 4, 5, 1,
                        NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
    starlight_draw_rect(fb, x + 3, y + 6, 5, 1,
                        NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
    starlight_draw_rect(fb, x + 3, y + 8, 5, 1,
                        NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
}

/* Draw a symlink icon */
static void nova_draw_link_icon(struct starlight_framebuffer *fb,
                                 int x, int y) {
    starlight_draw_rect(fb, x + 1, y, 9, 12,
                        NOVA_LINK_R, NOVA_LINK_G, NOVA_LINK_B);
    /* Arrow */
    starlight_draw_rect(fb, x + 3, y + 5, 5, 1,
                        NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
    starlight_draw_rect(fb, x + 6, y + 3, 1, 5,
                        NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
}

void nova_draw(struct starlight_framebuffer *fb, struct starlight_window *win,
               int cursor_x, int cursor_y) {
    struct nova_filemanager *fm = win->filemanager;
    if (!fm || !fm->active) return;

    int wx = win->x;
    int wy = win->y;
    int ww = win->width;
    int wh = win->height;

    /* Path bar */
    starlight_draw_rect(fb, wx, wy, ww, NOVA_PATHBAR_HEIGHT,
                        NOVA_PATHBAR_R, NOVA_PATHBAR_G, NOVA_PATHBAR_B);

    /* Path text */
    char path_display[80];
    strncpy(path_display, fm->current_path, sizeof(path_display) - 1);
    path_display[sizeof(path_display) - 1] = '\0';
    starlight_draw_text_simple(fb, wx + 8, wy + 9, path_display,
                               180, 170, 230);

    /* Toolbar */
    int toolbar_y = wy + NOVA_PATHBAR_HEIGHT;
    starlight_draw_rect(fb, wx, toolbar_y, ww, NOVA_TOOLBAR_HEIGHT,
                        NOVA_TOOLBAR_R, NOVA_TOOLBAR_G, NOVA_TOOLBAR_B);

    /* Up button */
    starlight_draw_rect(fb, wx + 4, toolbar_y + 4, 30, 20,
                        TASKBAR_BTN_R, TASKBAR_BTN_G, TASKBAR_BTN_B);
    starlight_draw_text_simple(fb, wx + 10, toolbar_y + 10, "Up",
                               200, 200, 230);

    /* Home button */
    starlight_draw_rect(fb, wx + 40, toolbar_y + 4, 42, 20,
                        TASKBAR_BTN_R, TASKBAR_BTN_G, TASKBAR_BTN_B);
    starlight_draw_text_simple(fb, wx + 46, toolbar_y + 10, "Home",
                               200, 200, 230);

    /* Entry count */
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d items", fm->entry_count);
    starlight_draw_text_simple(fb, wx + ww - 80, toolbar_y + 10,
                               count_buf, 130, 130, 160);

    /* Separator */
    int list_y = toolbar_y + NOVA_TOOLBAR_HEIGHT;
    starlight_draw_rect(fb, wx, list_y, ww, 1,
                        BORDER_R, BORDER_G, BORDER_B);
    list_y += 1;

    /* Column headers */
    starlight_draw_rect(fb, wx, list_y, ww, NOVA_ENTRY_HEIGHT,
                        NOVA_TOOLBAR_R, NOVA_TOOLBAR_G, NOVA_TOOLBAR_B);
    starlight_draw_text_simple(fb, wx + NOVA_NAME_COL_X, list_y + 7,
                               "Name", 140, 140, 170);
    starlight_draw_text_simple(fb, wx + NOVA_SIZE_COL_X, list_y + 7,
                               "Size", 140, 140, 170);
    list_y += NOVA_ENTRY_HEIGHT;

    /* Calculate visible rows */
    int list_area_height = (wy + wh) - list_y;
    fm->visible_rows = list_area_height / NOVA_ENTRY_HEIGHT;
    if (fm->visible_rows < 1) fm->visible_rows = 1;

    /* Update hover based on cursor */
    fm->hover = -1;
    if (cursor_x >= wx && cursor_x < wx + ww &&
        cursor_y >= list_y && cursor_y < list_y + fm->visible_rows * NOVA_ENTRY_HEIGHT) {
        int row = (cursor_y - list_y) / NOVA_ENTRY_HEIGHT;
        int idx = row + fm->scroll_offset;
        if (idx >= 0 && idx < fm->entry_count) {
            fm->hover = idx;
        }
    }

    /* Draw entries */
    for (int i = 0; i < fm->visible_rows; i++) {
        int idx = i + fm->scroll_offset;
        if (idx >= fm->entry_count) break;

        struct nova_entry *e = &fm->entries[idx];
        int ey = list_y + i * NOVA_ENTRY_HEIGHT;

        /* Background (selected, hovered, or normal) */
        if (idx == fm->selected) {
            starlight_draw_rect(fb, wx, ey, ww, NOVA_ENTRY_HEIGHT,
                               NOVA_ENTRY_SELECTED_R, NOVA_ENTRY_SELECTED_G,
                               NOVA_ENTRY_SELECTED_B);
        } else if (idx == fm->hover) {
            starlight_draw_rect(fb, wx, ey, ww, NOVA_ENTRY_HEIGHT,
                               NOVA_ENTRY_HOVER_R, NOVA_ENTRY_HOVER_G,
                               NOVA_ENTRY_HOVER_B);
        }

        /* Icon */
        int icon_x = wx + 4;
        int icon_y = ey + 4;
        if (strcmp(e->name, "..") == 0) {
            nova_draw_folder_icon(fb, icon_x, icon_y);
        } else if (e->is_link) {
            nova_draw_link_icon(fb, icon_x, icon_y);
        } else if (e->is_dir) {
            nova_draw_folder_icon(fb, icon_x, icon_y);
        } else {
            nova_draw_file_icon(fb, icon_x, icon_y);
        }

        /* Name */
        char display_name[44];
        strncpy(display_name, e->name, 40);
        display_name[40] = '\0';
        if (strlen(e->name) > 40) {
            display_name[37] = '.';
            display_name[38] = '.';
            display_name[39] = '.';
            display_name[40] = '\0';
        }

        uint8_t nr, ng, nb;
        if (e->is_dir) {
            nr = NOVA_DIR_R; ng = NOVA_DIR_G; nb = NOVA_DIR_B;
        } else if (e->is_link) {
            nr = NOVA_LINK_R; ng = NOVA_LINK_G; nb = NOVA_LINK_B;
        } else {
            nr = NOVA_FILE_R; ng = NOVA_FILE_G; nb = NOVA_FILE_B;
        }

        starlight_draw_text_simple(fb, wx + NOVA_NAME_COL_X, ey + 7,
                                   display_name, nr, ng, nb);

        /* Size (only for files) */
        if (!e->is_dir && strcmp(e->name, "..") != 0) {
            char size_buf[16];
            nova_format_size(e->size, size_buf, sizeof(size_buf));
            starlight_draw_text_simple(fb, wx + NOVA_SIZE_COL_X, ey + 7,
                                       size_buf, 130, 130, 160);
        } else if (e->is_dir && strcmp(e->name, "..") != 0) {
            starlight_draw_text_simple(fb, wx + NOVA_SIZE_COL_X, ey + 7,
                                       "<dir>", 100, 100, 140);
        }
    }

    /* Scrollbar if needed */
    if (fm->entry_count > fm->visible_rows) {
        int sb_x = wx + ww - 8;
        int sb_h = list_area_height;
        int sb_y = list_y;

        /* Track */
        starlight_draw_rect(fb, sb_x, sb_y, 8, sb_h,
                           NOVA_PATHBAR_R, NOVA_PATHBAR_G, NOVA_PATHBAR_B);

        /* Thumb */
        int thumb_h = (fm->visible_rows * sb_h) / fm->entry_count;
        if (thumb_h < 20) thumb_h = 20;
        int thumb_y = sb_y + (fm->scroll_offset * (sb_h - thumb_h)) /
                      (fm->entry_count - fm->visible_rows);

        starlight_draw_rect(fb, sb_x + 1, thumb_y, 6, thumb_h,
                           BORDER_R, BORDER_G, BORDER_B);
    }
}

int nova_handle_click(struct starlight_server *server, int win_id,
                      int local_x, int local_y) {
    struct starlight_window *win = &server->windows[win_id];
    struct nova_filemanager *fm = win->filemanager;
    if (!fm || !fm->active) return 0;

    int ww = win->width;

    /* Check toolbar buttons */
    int toolbar_y = NOVA_PATHBAR_HEIGHT;

    /* Up button */
    if (local_x >= 4 && local_x < 34 &&
        local_y >= toolbar_y + 4 && local_y < toolbar_y + 24) {
        nova_navigate_up(fm);
        snprintf(win->title, sizeof(win->title), "Nova - %s", fm->current_path);
        return 1;
    }

    /* Home button */
    if (local_x >= 40 && local_x < 82 &&
        local_y >= toolbar_y + 4 && local_y < toolbar_y + 24) {
        nova_navigate(fm, "/root");
        snprintf(win->title, sizeof(win->title), "Nova - %s", fm->current_path);
        return 1;
    }

    /* Check file list area */
    int list_y = toolbar_y + NOVA_TOOLBAR_HEIGHT + 1 + NOVA_ENTRY_HEIGHT;
    (void)ww;

    if (local_y >= list_y) {
        int row = (local_y - list_y) / NOVA_ENTRY_HEIGHT;
        int idx = row + fm->scroll_offset;

        if (idx >= 0 && idx < fm->entry_count) {
            if (fm->selected == idx) {
                /* Double-click effect: enter directory */
                nova_enter_selected(server, win_id);
            } else {
                fm->selected = idx;
            }
            return 1;
        }
    }

    return 0;
}

int nova_create_window(struct starlight_server *server,
                       int x, int y, int w, int h, const char *path) {
    char title[64];
    snprintf(title, sizeof(title), "Nova - %s", path);

    int id = starlight_window_create(server, x, y, w, h, title,
                                     NOVA_BG_R, NOVA_BG_G, NOVA_BG_B);
    if (id < 0) return -1;

    struct nova_filemanager *fm = malloc(sizeof(struct nova_filemanager));
    if (!fm) {
        starlight_window_close(server, id);
        return -1;
    }

    if (nova_init(fm, path) < 0) {
        free(fm);
        starlight_window_close(server, id);
        return -1;
    }

    server->windows[id].filemanager = fm;
    return id;
}
