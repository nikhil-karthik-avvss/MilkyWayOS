/*
 * Pulsar — Terminal emulator for MilkyWayOS
 * Runs inside a Starlight window
 *
 * Features:
 * - PTY-based shell spawning (/bin/bash)
 * - Basic ANSI escape sequence handling
 * - Scrolling text buffer
 * - Blinking cursor
 * - Keyboard input forwarding
 */

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/input-event-codes.h>
#include "starlight.h"

/* Key code to ASCII mapping */
static const char keymap_normal[] = {
    [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
    [KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
    [KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
    [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
    [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
    [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
    [KEY_Y] = 'y', [KEY_Z] = 'z',
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4',
    [KEY_5] = '5', [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8',
    [KEY_9] = '9', [KEY_0] = '0',
    [KEY_SPACE] = ' ', [KEY_MINUS] = '-', [KEY_EQUAL] = '=',
    [KEY_LEFTBRACE] = '[', [KEY_RIGHTBRACE] = ']',
    [KEY_BACKSLASH] = '\\', [KEY_SEMICOLON] = ';', [KEY_APOSTROPHE] = '\'',
    [KEY_GRAVE] = '`', [KEY_COMMA] = ',', [KEY_DOT] = '.',
    [KEY_SLASH] = '/',
};

static const char keymap_shift[] = {
    [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D',
    [KEY_E] = 'E', [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H',
    [KEY_I] = 'I', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
    [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O', [KEY_P] = 'P',
    [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
    [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X',
    [KEY_Y] = 'Y', [KEY_Z] = 'Z',
    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$',
    [KEY_5] = '%', [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*',
    [KEY_9] = '(', [KEY_0] = ')',
    [KEY_SPACE] = ' ', [KEY_MINUS] = '_', [KEY_EQUAL] = '+',
    [KEY_LEFTBRACE] = '{', [KEY_RIGHTBRACE] = '}',
    [KEY_BACKSLASH] = '|', [KEY_SEMICOLON] = ':', [KEY_APOSTROPHE] = '"',
    [KEY_GRAVE] = '~', [KEY_COMMA] = '<', [KEY_DOT] = '>',
    [KEY_SLASH] = '?',
};

/* Scroll the terminal up by one line */
static void pulsar_scroll_up(struct pulsar_terminal *term) {
    term->cursor_row++;
    if (term->cursor_row >= PULSAR_SCROLLBACK) {
        /* Wrap around — shift everything up */
        memmove(&term->grid[0], &term->grid[1],
                (PULSAR_SCROLLBACK - 1) * sizeof(term->grid[0]));
        term->cursor_row = PULSAR_SCROLLBACK - 1;

        /* Clear the new bottom row */
        for (int c = 0; c < term->cols; c++) {
            term->grid[term->cursor_row][c].ch = ' ';
            term->grid[term->cursor_row][c].fg_r = PULSAR_FG_R;
            term->grid[term->cursor_row][c].fg_g = PULSAR_FG_G;
            term->grid[term->cursor_row][c].fg_b = PULSAR_FG_B;
            term->grid[term->cursor_row][c].bg_r = 0;
            term->grid[term->cursor_row][c].bg_g = 0;
            term->grid[term->cursor_row][c].bg_b = 0;
        }
    }

    if (term->total_rows < PULSAR_SCROLLBACK)
        term->total_rows++;

    /* Keep scroll view at bottom */
    term->scroll_top = term->cursor_row - term->rows + 1;
    if (term->scroll_top < 0) term->scroll_top = 0;
}

/* Process a single character from shell output */
static void pulsar_put_char(struct pulsar_terminal *term, char ch) {
    if (ch == '\n') {
        pulsar_scroll_up(term);
        term->cursor_col = 0;
        return;
    }

    if (ch == '\r') {
        term->cursor_col = 0;
        return;
    }

    if (ch == '\t') {
        int next = ((term->cursor_col / PULSAR_TAB_WIDTH) + 1) * PULSAR_TAB_WIDTH;
        if (next > term->cols) next = term->cols;
        term->cursor_col = next;
        return;
    }

    if (ch == '\b' || ch == 127) {
        if (term->cursor_col > 0) term->cursor_col--;
        return;
    }

    if (ch == '\a') {
        /* Bell — ignore */
        return;
    }

    /* Regular printable character */
    if (term->cursor_col >= term->cols) {
        pulsar_scroll_up(term);
        term->cursor_col = 0;
    }

    struct pulsar_cell *cell = &term->grid[term->cursor_row][term->cursor_col];
    cell->ch = ch;
    cell->fg_r = PULSAR_FG_R;
    cell->fg_g = PULSAR_FG_G;
    cell->fg_b = PULSAR_FG_B;
    cell->bg_r = 0;
    cell->bg_g = 0;
    cell->bg_b = 0;

    term->cursor_col++;
}

/* Handle a complete CSI escape sequence */
static void pulsar_handle_csi(struct pulsar_terminal *term) {
    int params[8] = {0};
    int param_count = 0;
    int i = 0;

    /* Parse parameters */
    while (i < term->esc_len && param_count < 8) {
        if (term->esc_buf[i] >= '0' && term->esc_buf[i] <= '9') {
            params[param_count] = params[param_count] * 10 +
                                  (term->esc_buf[i] - '0');
        } else if (term->esc_buf[i] == ';') {
            param_count++;
        } else {
            break;
        }
        i++;
    }
    param_count++;

    if (i >= term->esc_len) return;
    char cmd = term->esc_buf[i];

    switch (cmd) {
    case 'A': /* Cursor up */
        term->cursor_row -= (params[0] ? params[0] : 1);
        if (term->cursor_row < 0) term->cursor_row = 0;
        break;

    case 'B': /* Cursor down */
        term->cursor_row += (params[0] ? params[0] : 1);
        break;

    case 'C': /* Cursor forward */
        term->cursor_col += (params[0] ? params[0] : 1);
        if (term->cursor_col >= term->cols) term->cursor_col = term->cols - 1;
        break;

    case 'D': /* Cursor back */
        term->cursor_col -= (params[0] ? params[0] : 1);
        if (term->cursor_col < 0) term->cursor_col = 0;
        break;

    case 'H': /* Cursor position */
    case 'f': {
        int row = (params[0] ? params[0] : 1) - 1;
        int col = (param_count > 1 && params[1]) ? params[1] - 1 : 0;
        /* Make relative to scroll region */
        term->cursor_row = term->scroll_top + row;
        term->cursor_col = col;
        if (term->cursor_col >= term->cols) term->cursor_col = term->cols - 1;
        break;
    }

    case 'J': /* Erase in display */
        if (params[0] == 2 || params[0] == 3) {
            /* Clear entire screen */
            for (int r = term->scroll_top; r < term->scroll_top + term->rows; r++) {
                for (int c = 0; c < term->cols; c++) {
                    term->grid[r][c].ch = ' ';
                    term->grid[r][c].fg_r = PULSAR_FG_R;
                    term->grid[r][c].fg_g = PULSAR_FG_G;
                    term->grid[r][c].fg_b = PULSAR_FG_B;
                }
            }
            term->cursor_row = term->scroll_top;
            term->cursor_col = 0;
        } else if (params[0] == 0) {
            /* Clear from cursor to end */
            for (int c = term->cursor_col; c < term->cols; c++) {
                term->grid[term->cursor_row][c].ch = ' ';
            }
            for (int r = term->cursor_row + 1; r < term->scroll_top + term->rows; r++) {
                for (int c = 0; c < term->cols; c++) {
                    term->grid[r][c].ch = ' ';
                }
            }
        }
        break;

    case 'K': /* Erase in line */
        if (params[0] == 0) {
            /* Clear from cursor to end of line */
            for (int c = term->cursor_col; c < term->cols; c++) {
                term->grid[term->cursor_row][c].ch = ' ';
            }
        } else if (params[0] == 1) {
            /* Clear from start to cursor */
            for (int c = 0; c <= term->cursor_col; c++) {
                term->grid[term->cursor_row][c].ch = ' ';
            }
        } else if (params[0] == 2) {
            /* Clear entire line */
            for (int c = 0; c < term->cols; c++) {
                term->grid[term->cursor_row][c].ch = ' ';
            }
        }
        break;

    case 'm': /* SGR — text attributes (simplified) */
        /* We ignore color codes for now, just reset on 0 */
        break;

    case 'r': /* Set scrolling region — ignore for simplicity */
        break;

    case 'h': /* Set mode — ignore */
    case 'l': /* Reset mode — ignore */
        break;

    default:
        break;
    }
}

/* Process escape sequences from shell output */
static void pulsar_process_char(struct pulsar_terminal *term, char ch) {
    switch (term->esc_state) {
    case 0: /* Normal mode */
        if (ch == '\033') {
            term->esc_state = 1;
            term->esc_len = 0;
        } else {
            pulsar_put_char(term, ch);
        }
        break;

    case 1: /* Got ESC */
        if (ch == '[') {
            term->esc_state = 2;
        } else if (ch == ']') {
            /* OSC sequence (window title etc) — skip until BEL or ST */
            term->esc_state = 3;
        } else if (ch == '(') {
            /* Character set — skip next char */
            term->esc_state = 4;
        } else {
            term->esc_state = 0;
        }
        break;

    case 2: /* CSI sequence */
        if (term->esc_len < (int)sizeof(term->esc_buf) - 1) {
            term->esc_buf[term->esc_len++] = ch;
        }
        /* Check if this is the final byte */
        if ((ch >= '@' && ch <= '~') && ch != '[') {
            term->esc_buf[term->esc_len] = '\0';
            pulsar_handle_csi(term);
            term->esc_state = 0;
        }
        break;

    case 3: /* OSC sequence — skip until BEL or ST */
        if (ch == '\a') {
            term->esc_state = 0;
        } else if (ch == '\033') {
            /* Possible ST (\033\\) */
            term->esc_state = 5;
        }
        break;

    case 4: /* Character set designation — skip one char */
        term->esc_state = 0;
        break;

    case 5: /* Got ESC inside OSC, check for backslash */
        term->esc_state = (ch == '\\') ? 0 : 3;
        break;
    }
}

int pulsar_init(struct pulsar_terminal *term, int cols, int rows) {
    memset(term, 0, sizeof(*term));
    term->cols = cols;
    term->rows = rows;
    term->cursor_col = 0;
    term->cursor_row = 0;
    term->scroll_top = 0;
    term->total_rows = rows;
    term->active = 1;
    term->esc_state = 0;

    /* Clear the grid */
    for (int r = 0; r < PULSAR_SCROLLBACK; r++) {
        for (int c = 0; c < PULSAR_MAX_COLS; c++) {
            term->grid[r][c].ch = ' ';
            term->grid[r][c].fg_r = PULSAR_FG_R;
            term->grid[r][c].fg_g = PULSAR_FG_G;
            term->grid[r][c].fg_b = PULSAR_FG_B;
            term->grid[r][c].bg_r = 0;
            term->grid[r][c].bg_g = 0;
            term->grid[r][c].bg_b = 0;
        }
    }

    /* Open PTY */
    term->master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (term->master_fd < 0) {
        fprintf(stderr, "[Pulsar] Failed to open PTY: %s\n", strerror(errno));
        return -1;
    }

    if (grantpt(term->master_fd) < 0 || unlockpt(term->master_fd) < 0) {
        fprintf(stderr, "[Pulsar] Failed to setup PTY: %s\n", strerror(errno));
        close(term->master_fd);
        return -1;
    }

    char *slave_name = ptsname(term->master_fd);
    if (!slave_name) {
        fprintf(stderr, "[Pulsar] Failed to get PTY slave name\n");
        close(term->master_fd);
        return -1;
    }

    /* Set non-blocking on master */
    int flags = fcntl(term->master_fd, F_GETFL);
    fcntl(term->master_fd, F_SETFL, flags | O_NONBLOCK);

    /* Fork the shell */
    term->child_pid = fork();
    if (term->child_pid < 0) {
        fprintf(stderr, "[Pulsar] Fork failed: %s\n", strerror(errno));
        close(term->master_fd);
        return -1;
    }

    if (term->child_pid == 0) {
        /* Child process — become the shell */
        close(term->master_fd);

        /* Create new session */
        setsid();

        /* Open slave PTY */
        int slave_fd = open(slave_name, O_RDWR);
        if (slave_fd < 0) {
            _exit(1);
        }

        /* Set terminal size */
        struct winsize ws = {
            .ws_row = rows,
            .ws_col = cols,
        };
        ioctl(slave_fd, TIOCSWINSZ, &ws);

        /* Redirect stdio */
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        if (slave_fd > 2) close(slave_fd);

        /* Set environment */
        setenv("TERM", "dumb", 1);
        setenv("HOME", "/root", 1);
        setenv("PS1", "milkyway$ ", 1);
        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

        /* Execute shell */
        execlp("/bin/bash", "bash", "--norc", "--noprofile", NULL);
        _exit(1);
    }

    printf("[Pulsar] Terminal started (pid=%d, cols=%d, rows=%d)\n",
           term->child_pid, cols, rows);
    return 0;
}

void pulsar_destroy(struct pulsar_terminal *term) {
    if (term->child_pid > 0) {
        kill(term->child_pid, SIGTERM);
        waitpid(term->child_pid, NULL, WNOHANG);
    }
    if (term->master_fd >= 0) {
        close(term->master_fd);
    }
    term->active = 0;
}

void pulsar_process_output(struct pulsar_terminal *term) {
    if (!term->active || term->master_fd < 0) return;

    /* Check if child is still alive */
    int status;
    pid_t ret = waitpid(term->child_pid, &status, WNOHANG);
    if (ret > 0) {
        term->active = 0;
        return;
    }

    /* Read output from shell */
    char buf[4096];
    ssize_t n = read(term->master_fd, buf, sizeof(buf));
    if (n > 0) {
        for (ssize_t i = 0; i < n; i++) {
            pulsar_process_char(term, buf[i]);
        }
    }
}

void pulsar_send_key(struct pulsar_terminal *term, uint32_t key, int shift) {
    if (!term->active || term->master_fd < 0) return;

    char buf[8];
    int len = 0;

    switch (key) {
    case KEY_ENTER:
        buf[0] = '\n';
        len = 1;
        break;

    case KEY_BACKSPACE:
        buf[0] = 127;
        len = 1;
        break;

    case KEY_TAB:
        buf[0] = '\t';
        len = 1;
        break;

    case KEY_UP:
        memcpy(buf, "\033[A", 3);
        len = 3;
        break;

    case KEY_DOWN:
        memcpy(buf, "\033[B", 3);
        len = 3;
        break;

    case KEY_RIGHT:
        memcpy(buf, "\033[C", 3);
        len = 3;
        break;

    case KEY_LEFT:
        memcpy(buf, "\033[D", 3);
        len = 3;
        break;

    case KEY_HOME:
        memcpy(buf, "\033[H", 3);
        len = 3;
        break;

    case KEY_END:
        memcpy(buf, "\033[F", 3);
        len = 3;
        break;

    case KEY_DELETE:
        memcpy(buf, "\033[3~", 4);
        len = 4;
        break;

    default:
        if (key < sizeof(keymap_normal)) {
            char ch;
            if (shift && key < sizeof(keymap_shift)) {
                ch = keymap_shift[key];
            } else {
                ch = keymap_normal[key];
            }
            if (ch) {
                buf[0] = ch;
                len = 1;
            }
        }
        break;
    }

    if (len > 0) {
        write(term->master_fd, buf, len);
    }
}

void pulsar_draw(struct starlight_framebuffer *fb,
                 struct starlight_window *win) {
    struct pulsar_terminal *term = win->terminal;
    if (!term) return;

    /* Draw terminal background (already done by window draw) */

    int start_row = term->scroll_top;
    int end_row = start_row + term->rows;
    if (end_row > PULSAR_SCROLLBACK) end_row = PULSAR_SCROLLBACK;

    for (int r = start_row; r < end_row; r++) {
        int screen_row = r - start_row;
        for (int c = 0; c < term->cols; c++) {
            struct pulsar_cell *cell = &term->grid[r][c];
            if (cell->ch <= ' ' && cell->ch != ' ') continue;

            int px = win->x + 4 + c * PULSAR_FONT_W;
            int py = win->y + 4 + screen_row * PULSAR_FONT_H;

            if (cell->ch != ' ') {
                starlight_draw_char(fb, px, py, cell->ch,
                                   cell->fg_r, cell->fg_g, cell->fg_b);
            }
        }
    }

    /* Draw cursor (blinking effect using simple frame counter) */
    static int blink = 0;
    blink++;
    if ((blink / 30) % 2 == 0) {
        int cx = win->x + 4 + term->cursor_col * PULSAR_FONT_W;
        int cy = win->y + 4 + (term->cursor_row - term->scroll_top) * PULSAR_FONT_H;

        if (cy >= win->y && cy + PULSAR_FONT_H <= win->y + win->height) {
            starlight_draw_rect(fb, cx, cy, PULSAR_FONT_W, PULSAR_FONT_H,
                               PULSAR_CURSOR_R, PULSAR_CURSOR_G,
                               PULSAR_CURSOR_B);
        }
    }
}

int pulsar_create_window(struct starlight_server *server,
                         int x, int y, int w, int h) {
    int cols = (w - 8) / PULSAR_FONT_W;
    int rows = (h - 8) / PULSAR_FONT_H;

    if (cols > PULSAR_MAX_COLS) cols = PULSAR_MAX_COLS;
    if (rows > PULSAR_MAX_ROWS) rows = PULSAR_MAX_ROWS;
    if (cols < 10) cols = 10;
    if (rows < 3) rows = 3;

    int id = starlight_window_create(server, x, y, w, h,
                                     "Pulsar", 8, 8, 20);
    if (id < 0) return -1;

    struct pulsar_terminal *term = malloc(sizeof(struct pulsar_terminal));
    if (!term) {
        starlight_window_close(server, id);
        return -1;
    }

    if (pulsar_init(term, cols, rows) < 0) {
        free(term);
        starlight_window_close(server, id);
        return -1;
    }

    server->windows[id].terminal = term;
    return id;
}
