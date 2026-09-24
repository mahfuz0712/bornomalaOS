#include "apps.h"
#include "compositor.h"
#include "../alloc.h"
#include "../klib.h"
#include "../console.h"
#include "../input.h"
#include "../clipboard.h"
#include "../interrupts.h"
#include "uistate.h"
#include "notify.h"

/* ════════════════════════════════════════════════════════════════════════════
   shared widgets (Windows-7 style)
   ════════════════════════════════════════════════════════════════════════════ */
#define COL_TEXT   RGB(0x1C, 0x2A, 0x36)
#define COL_LINK   RGB(0x24, 0x52, 0x7A)

static void bar_background(gfx_t *g, int x, int y, int w, int h) {
    paint_t p = paint_v2(RGB(0xF5, 0xFB, 0xFF), RGB(0xD4, 0xE6, 0xF2));
    gfx_fill_paint(g, x, y, w, h, &p);
    gfx_hline(g, x, y + h - 1, w, RGB(0xA5, 0xBD, 0xCD));
}

/* returns the x after the button */
static int toolbar_button(gfx_t *g, int x, int y, const char *label) {
    int w = font_text_width(&font_ui12, label) + 18;
    paint_t p = paint_v2(RGB(0xFF, 0xFF, 0xFF), RGB(0xD6, 0xE5, 0xEF));
    gfx_fill_round_paint(g, x, y, w, 27, 3, &p);
    gfx_stroke_round_rect(g, x, y, w, 27, 3, RGB(0x9A, 0xB1, 0xC1));
    gfx_text(g, &font_ui12, x + 9, text_baseline_in(&font_ui12, y, 27), label, COL_TEXT);
    return x + w + 5;
}

static void toolbar(gfx_t *g, rect_t c, const char *b1, const char *b2, const char *crumb) {
    bar_background(g, c.x, c.y, c.w, 39);
    int x = c.x + 5;
    x = toolbar_button(g, x, c.y + 6, b1);
    x = toolbar_button(g, x, c.y + 6, b2);
    int cw = c.x + c.w - 5 - 36 - x;
    gfx_fill_round_rect(g, x, c.y + 6, cw, 27, 2, RGB(255, 255, 255));
    gfx_stroke_round_rect(g, x, c.y + 6, cw, 27, 2, RGB(0x9E, 0xB3, 0xC1));
    gfx_text(g, &font_ui12, x + 9, text_baseline_in(&font_ui12, c.y + 6, 27), crumb, COL_TEXT);
    int sx = c.x + c.w - 5 - 31;
    paint_t p = paint_v2(RGB(0xFF, 0xFF, 0xFF), RGB(0xD6, 0xE5, 0xEF));
    gfx_fill_round_paint(g, sx, c.y + 6, 31, 27, 3, &p);
    gfx_stroke_round_rect(g, sx, c.y + 6, 31, 27, 3, RGB(0x9A, 0xB1, 0xC1));
    sym_draw(g, SYM_SEARCH, sx + 15, c.y + 19, 14, RGB(0x35, 0x55, 0x70));
}

static void sidebar(gfx_t *g, rect_t area, const char *const *entries, int n) {
    paint_t p = paint_diag(RGB(0xEA, 0xF5, 0xFB), RGB(0xD5, 0xE9, 0xF5));
    gfx_fill_paint(g, area.x, area.y, 175, area.h, &p);
    gfx_vline(g, area.x + 174, area.y, area.h, RGB(0xB4, 0xC9, 0xD6));
    int y = area.y + 18;
    for (int i = 0; i < n; i++) {
        const char *e = entries[i];
        if (e[0] == '#') {                                   /* section title */
            gfx_text(g, &font_ui13b, area.x + 16, y + 12, e + 1, COL_LINK);
            y += 24;
        } else {
            gfx_fill_round_rect(g, area.x + 17, y + 5, 8, 8, 2, RGB(0x5B, 0x9B, 0xD1));
            gfx_text(g, &font_ui12, area.x + 32, y + 14, e, COL_TEXT);
            y += 24;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Computer
   ════════════════════════════════════════════════════════════════════════════ */
static void drive_tile(gfx_t *g, int x, int y, const char *label, int percent, bool disc) {
    if (disc) {
        gfx_fill_circle(g, x + 32, y + 24, 22, RGB(0xC9, 0xD5, 0xDE));
        gfx_fill_circle(g, x + 32, y + 24, 20, RGB(0xE6, 0xEE, 0xF4));
        gfx_fill_circle(g, x + 32, y + 24, 6, RGB(0x9F, 0xB0, 0xBD));
        gfx_fill_circle(g, x + 32, y + 24, 2, RGB(0xF5, 0xF9, 0xFC));
    } else {
        icon_draw(g, ICON_DRIVE, x + 8, y, 48);
    }
    gfx_text_center(g, &font_ui12, x + 32, y + 66, label, COL_TEXT);
    if (percent >= 0) {
        gfx_fill_round_rect(g, x + 2, y + 74, 60, 10, 2, RGB(0xE6, 0xE6, 0xE6));
        gfx_stroke_round_rect(g, x + 2, y + 74, 60, 10, 2, RGB(0xAC, 0xAC, 0xAC));
        paint_t fill = paint_v2(RGB(0x5F, 0xD0, 0x6A), RGB(0x21, 0xA0, 0x35));
        gfx_fill_round_paint(g, x + 3, y + 75, 58 * percent / 100, 8, 1, &fill);
    }
}

static void computer_paint(gfx_t *g, window_t *w, rect_t c) {
    (void)w;
    gfx_fill_rect(g, c.x, c.y, c.w, c.h, RGB(255, 255, 255));
    toolbar(g, c, "Organize", "System properties", "Computer");
    rect_t body = R(c.x, c.y + 39, c.w, c.h - 39);
    static const char *const side[] = { "#Favorites", "Desktop", "Downloads", "Recent Places",
                                        "#Libraries", "Documents", "Music", "Pictures" };
    sidebar(g, body, side, 8);
    int mx = body.x + 175 + 16, my = body.y + 10;
    gfx_text(g, &font_ui13b, mx, my + 14, "Hard Disk Drives", COL_LINK);
    gfx_hline(g, mx, my + 22, body.w - 175 - 32, RGB(0xC9, 0xD9, 0xE6));
    drive_tile(g, mx + 8, my + 34, "Local Disk (C:)", 42, false);
    drive_tile(g, mx + 8 + 130, my + 34, "Data (D:)", 25, false);
    int y2 = my + 34 + 108;
    gfx_text(g, &font_ui13b, mx, y2 + 14, "Devices with Removable Storage", COL_LINK);
    gfx_hline(g, mx, y2 + 22, body.w - 175 - 32, RGB(0xC9, 0xD9, 0xE6));
    drive_tile(g, mx + 8, y2 + 34, "DVD Drive (E:)", -1, true);
}

/* ════════════════════════════════════════════════════════════════════════════
   Files
   ════════════════════════════════════════════════════════════════════════════ */
typedef struct { int selected; char owner[40]; } files_state_t;

static const char *const file_names[4] = { "Projects", "University", "Dexten Academy", "Resume.docx" };

static rect_t files_item_rect(rect_t body, int i) {
    int mx = body.x + 175 + 16, my = body.y + 40;
    return R(mx + i * 100, my, 88, 92);
}

static void files_paint(gfx_t *g, window_t *w, rect_t c) {
    files_state_t *st = (files_state_t *)w->data;
    gfx_fill_rect(g, c.x, c.y, c.w, c.h, RGB(255, 255, 255));
    toolbar(g, c, "Organize", "New folder", "Libraries > Documents");
    rect_t body = R(c.x, c.y + 39, c.w, c.h - 39);
    static const char *const side[] = { "#Favorites", "Desktop", "Downloads", "Documents" };
    sidebar(g, body, side, 4);
    int mx = body.x + 175 + 16, my = body.y + 10;
    gfx_text(g, &font_ui13b, mx, my + 14, "Documents", COL_LINK);
    gfx_hline(g, mx, my + 22, body.w - 175 - 32, RGB(0xC9, 0xD9, 0xE6));
    for (int i = 0; i < 4; i++) {
        rect_t r = files_item_rect(body, i);
        if (i == st->selected) {
            gfx_fill_round_rect(g, r.x, r.y, r.w, r.h, 3, RGB(0xD8, 0xED, 0xFC));
            gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 3, RGB(0x7D, 0xA2, 0xCE));
        }
        icon_draw(g, i == 3 ? ICON_DOC : ICON_FOLDER, r.x + (r.w - 48) / 2, r.y + 6, 48);
        gfx_text_center(g, &font_ui12, r.x + r.w / 2, r.y + 74, file_names[i], COL_TEXT);
    }
}

static void files_mouse(window_t *w, int lx, int ly, int buttons, int prev) {
    if (!((buttons & 1) && !(prev & 1))) return;
    files_state_t *st = (files_state_t *)w->data;
    rect_t cl = window_client_size(w);
    rect_t body = R(0, 39, cl.w, cl.h - 39);
    int sel = -1;
    for (int i = 0; i < 4; i++)
        if (rect_contains(files_item_rect(body, i), lx, ly)) sel = i;
    if (sel != st->selected) { st->selected = sel; window_invalidate(w); }
}

static void free_data(window_t *w) { if (w->data) { kfree(w->data); w->data = 0; } }

/* ════════════════════════════════════════════════════════════════════════════
   Notepad  (selection, clipboard, undo, mouse + keyboard editing, scrolling, blinking caret)
   ════════════════════════════════════════════════════════════════════════════ */
#define NP_MAX 8192
#define NP_LINE_H 18
#define NP_UNDO 8

typedef struct { char buf[NP_MAX + 1]; int len, caret; } np_snapshot_t;

typedef struct {
    char buf[NP_MAX + 1];
    int  len, caret;
    int  sel_anchor;           /* -1 = no selection */
    int  scroll;               /* first visible line */
    int  want_col;
    bool caret_on;
    bool typing_run;           /* consecutive typing shares one undo step */
    int  undo_count;
    np_snapshot_t undo[NP_UNDO];
} notepad_t;

static const char *np_menu[] = { "File", "Edit", "Format", "View", "Help" };

static void np_line_col(const notepad_t *n, int pos, int *line, int *col) {
    int l = 0, c = 0;
    for (int i = 0; i < pos; i++) { if (n->buf[i] == '\n') { l++; c = 0; } else c++; }
    *line = l; *col = c;
}
static int np_line_start(const notepad_t *n, int line) {
    int l = 0;
    for (int i = 0; i < n->len; i++) { if (l == line) return i; if (n->buf[i] == '\n') l++; }
    return l == line ? n->len : -1;
}
static int np_line_len(const notepad_t *n, int start) {
    int i = start;
    while (i < n->len && n->buf[i] != '\n') i++;
    return i - start;
}
static void np_push_undo(notepad_t *n) {
    if (n->undo_count == NP_UNDO) {
        memmove(&n->undo[0], &n->undo[1], sizeof(np_snapshot_t) * (NP_UNDO - 1));
        n->undo_count--;
    }
    np_snapshot_t *s = &n->undo[n->undo_count++];
    memcpy(s->buf, n->buf, (size_t)n->len + 1);
    s->len = n->len; s->caret = n->caret;
}
static bool np_selection(const notepad_t *n, int *lo, int *hi) {
    if (n->sel_anchor < 0 || n->sel_anchor == n->caret) return false;
    *lo = n->sel_anchor < n->caret ? n->sel_anchor : n->caret;
    *hi = n->sel_anchor < n->caret ? n->caret : n->sel_anchor;
    return true;
}
static void np_delete_range(notepad_t *n, int lo, int hi) {
    memmove(n->buf + lo, n->buf + hi, (size_t)(n->len - hi) + 1);
    n->len -= hi - lo;
    n->caret = lo;
    n->sel_anchor = -1;
}
static void np_insert_str(notepad_t *n, const char *s, size_t len) {
    int lo, hi;
    if (np_selection(n, &lo, &hi)) np_delete_range(n, lo, hi);
    if ((size_t)n->len + len > NP_MAX) len = (size_t)(NP_MAX - n->len);
    memmove(n->buf + n->caret + len, n->buf + n->caret, (size_t)(n->len - n->caret) + 1);
    memcpy(n->buf + n->caret, s, len);
    n->caret += (int)len; n->len += (int)len;
}

static rect_t np_text_rect(rect_t c) { return R(c.x, c.y + 38, c.w, c.h - 38); }

static void np_scroll_into_view(notepad_t *n, rect_t c) {
    int line, col;
    np_line_col(n, n->caret, &line, &col);
    int rows = (np_text_rect(c).h - 12) / NP_LINE_H;
    if (rows < 1) rows = 1;
    if (line < n->scroll) n->scroll = line;
    if (line >= n->scroll + rows) n->scroll = line - rows + 1;
}

static void notepad_paint(gfx_t *g, window_t *w, rect_t c) {
    notepad_t *n = (notepad_t *)w->data;
    bar_background(g, c.x, c.y, c.w, 38);
    int x = c.x + 10;
    for (int i = 0; i < 5; i++) {
        gfx_text(g, &font_ui12, x, text_baseline_in(&font_ui12, c.y, 38), np_menu[i], COL_TEXT);
        x += font_text_width(&font_ui12, np_menu[i]) + 16;
    }
    rect_t t = np_text_rect(c);
    gfx_fill_rect(g, t.x, t.y, t.w, t.h, RGB(255, 255, 255));

    rect_t saved = gfx_push_clip(g, R(t.x + 2, t.y + 2, t.w - 4, t.h - 4));
    int cols = (t.w - 20) / 8;
    int rows = (t.h - 12) / NP_LINE_H + 1;
    int pos = np_line_start(n, n->scroll);
    if (pos < 0) pos = n->len;
    int cline, ccol, slo = 0, shi = 0;
    bool has_sel = np_selection(n, &slo, &shi);
    np_line_col(n, n->caret, &cline, &ccol);
    for (int r = 0; r < rows; r++) {
        int ll = np_line_len(n, pos);
        int y = t.y + 8 + r * NP_LINE_H;
        char tmp[200];
        int shown = ll < cols ? ll : cols;
        if (shown > (int)sizeof(tmp) - 1) shown = (int)sizeof(tmp) - 1;
        if (has_sel) {
            int s0 = slo > pos ? slo : pos, s1 = shi < pos + ll + 1 ? shi : pos + ll + 1;
            if (s0 < s1) {
                int c0 = s0 - pos, c1 = s1 - pos;
                if (c1 > cols) c1 = cols;
                gfx_fill_rect(g, t.x + 10 + c0 * 8, y - 1, (c1 - c0) * 8 + (s1 > pos + ll ? 4 : 0), 18, RGB(0xB5, 0xD5, 0xF5));
            }
        }
        memcpy(tmp, n->buf + pos, (size_t)shown);
        tmp[shown] = '\0';
        gfx_text_mono(g, t.x + 10, y, tmp, RGB(0x20, 0x20, 0x20));
        if (n->scroll + r == cline && n->caret_on && wm_active() == w)
            gfx_fill_rect(g, t.x + 10 + ccol * 8, y, 2, 16, RGB(0x10, 0x10, 0x10));
        pos += ll + 1;
        if (pos > n->len + 1) break;
    }
    gfx_pop_clip(g, saved);
}

static void np_touch(window_t *w, notepad_t *n) {
    n->caret_on = true;
    np_scroll_into_view(n, window_client_size(w));
    window_invalidate(w);
}

static void notepad_command(window_t *w, int cmd) {
    notepad_t *n = (notepad_t *)w->data;
    int lo, hi;
    switch (cmd) {
    case WM_CMD_COPY:
        if (np_selection(n, &lo, &hi)) clipboard_set(n->buf + lo, (size_t)(hi - lo));
        break;
    case WM_CMD_CUT:
        if (np_selection(n, &lo, &hi)) { clipboard_set(n->buf + lo, (size_t)(hi - lo)); np_push_undo(n); np_delete_range(n, lo, hi); n->typing_run = false; }
        break;
    case WM_CMD_PASTE: {
        size_t len; const char *cb = clipboard_get(&len);
        if (len) { np_push_undo(n); np_insert_str(n, cb, len); n->typing_run = false; }
        break; }
    case WM_CMD_UNDO:
        if (n->undo_count) {
            np_snapshot_t *s = &n->undo[--n->undo_count];
            memcpy(n->buf, s->buf, (size_t)s->len + 1);
            n->len = s->len; n->caret = s->caret; n->sel_anchor = -1; n->typing_run = false;
        }
        break;
    case WM_CMD_SELECT_ALL: n->sel_anchor = 0; n->caret = n->len; break;
    default: return;
    }
    np_touch(w, n);
}

static void notepad_key(window_t *w, uint16_t key, uint8_t mods) {
    notepad_t *n = (notepad_t *)w->data;
    int line, col, lo, hi;
    bool shift = (mods & MOD_SHIFT) != 0;
    if (mods & (MOD_CTRL | MOD_ALT)) return;

    bool nav = (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END);
    if (nav) {
        if (shift && n->sel_anchor < 0) n->sel_anchor = n->caret;
        if (!shift) n->sel_anchor = -1;
        n->typing_run = false;
        np_line_col(n, n->caret, &line, &col);
        if (key == KEY_LEFT) { if (n->caret > 0) n->caret--; n->want_col = -1; }
        else if (key == KEY_RIGHT) { if (n->caret < n->len) n->caret++; n->want_col = -1; }
        else if (key == KEY_HOME || key == KEY_END) {
            int s = np_line_start(n, line);
            n->caret = (key == KEY_HOME) ? s : s + np_line_len(n, s);
            n->want_col = -1;
        } else {
            if (n->want_col < 0) n->want_col = col;
            int target = line + (key == KEY_UP ? -1 : 1);
            int s = target >= 0 ? np_line_start(n, target) : -1;
            if (s >= 0) { int ll = np_line_len(n, s); n->caret = s + (n->want_col < ll ? n->want_col : ll); }
        }
        np_touch(w, n);
        return;
    }

    if ((key >= 32 && key < 127) || key == '\n' || key == '\t') {
        if (!n->typing_run || np_selection(n, &lo, &hi)) np_push_undo(n);
        n->typing_run = (key != '\n');
        if (key == '\t') np_insert_str(n, "    ", 4);
        else { char ch = (char)key; np_insert_str(n, &ch, 1); }
    } else if (key == KEY_BACKSPACE || key == KEY_DELETE) {
        np_push_undo(n);
        n->typing_run = false;
        if (np_selection(n, &lo, &hi)) np_delete_range(n, lo, hi);
        else if (key == KEY_BACKSPACE && n->caret > 0) np_delete_range(n, n->caret - 1, n->caret);
        else if (key == KEY_DELETE && n->caret < n->len) np_delete_range(n, n->caret, n->caret + 1);
        else if (n->undo_count) n->undo_count--;              /* nothing changed: drop the empty step */
    } else return;

    n->want_col = -1;
    np_touch(w, n);
}

static int np_pos_at(window_t *w, int lx, int ly) {
    notepad_t *n = (notepad_t *)w->data;
    rect_t t = np_text_rect(window_client_size(w));
    int rel = ly - t.y - 8;
    int line = n->scroll + (rel < 0 ? 0 : rel / NP_LINE_H);
    int s = np_line_start(n, line);
    if (s < 0) { int last, dummy; np_line_col(n, n->len, &last, &dummy); s = np_line_start(n, last); }
    int col = (lx - t.x - 10 + 4) / 8;
    if (col < 0) col = 0;
    int ll = np_line_len(n, s);
    return s + (col < ll ? col : ll);
}

static void notepad_mouse(window_t *w, int lx, int ly, int buttons, int prev) {
    notepad_t *n = (notepad_t *)w->data;
    rect_t t = np_text_rect(window_client_size(w));
    bool down = (buttons & 1) && !(prev & 1);
    if (down && ly < t.y) return;                            /* menu bar: not implemented */
    if (down) {
        n->caret = np_pos_at(w, lx, ly);
        n->sel_anchor = n->caret;
        n->typing_run = false; n->want_col = -1;
        np_touch(w, n);
    } else if (buttons & 1) {                                /* drag = extend the selection */
        int p = np_pos_at(w, lx, ly);
        if (p != n->caret) { n->caret = p; np_touch(w, n); }
    } else if (prev & 1) {
        if (n->sel_anchor == n->caret) n->sel_anchor = -1;
    }
}

static const char *np_initial =
    "BornomalaOS Notepad\n\n"
    "Click and drag to select. Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+Z / Ctrl+A work\n"
    "through the system shortcut manager; Shift+arrows extend the selection.\n";

/* ════════════════════════════════════════════════════════════════════════════
   Calculator  (fixed-point, 4 decimals, overflow-checked)
   ════════════════════════════════════════════════════════════════════════════ */
#define CALC_SCALE 10000LL
#define CALC_BTNS  18            /* 16 grid cells + wide "0" + "." */

typedef struct {
    int64_t acc;            /* left operand / result, scaled by CALC_SCALE */
    int64_t cur;            /* value shown (what was typed, or the last result) */
    char    op;             /* pending operator, 0 = none */
    bool    typing;         /* digits are being entered into `in` */
    char    in[20];         /* text being typed, e.g. "12.5" */
    bool    error;
    int     pressed;        /* button under the mouse button, -1 none */
} calc_t;

static const char calc_keys[CALC_BTNS] = {
    'C', '<', '/', 'x',
    '7', '8', '9', '-',
    '4', '5', '6', '+',
    '1', '2', '3', '=',
    '0', '.'
};
static const char *const calc_labels[CALC_BTNS] = {
    "C", "Del", "/", "x",
    "7", "8", "9", "-",
    "4", "5", "6", "+",
    "1", "2", "3", "=",
    "0", "."
};

static int64_t calc_parse(const char *s) {
    int64_t ip = 0, fp = 0, unit = CALC_SCALE / 10;
    bool point = false;
    for (; *s; s++) {
        if (*s == '.') { point = true; continue; }
        if (*s < '0' || *s > '9') continue;
        if (!point) ip = ip * 10 + (*s - '0');
        else if (unit > 0) { fp += (*s - '0') * unit; unit /= 10; }
    }
    return ip * CALC_SCALE + fp;
}

static void calc_reset(calc_t *c) {
    memset(c, 0, sizeof(*c));
    c->pressed = -1;
}

/* acc = acc <op> cur ; false on overflow / divide by zero */
static bool calc_compute(calc_t *c) {
    int64_t r = c->acc, b = c->cur, t;
    switch (c->op) {
    case '+': if (__builtin_add_overflow(r, b, &r)) return false; break;
    case '-': if (__builtin_sub_overflow(r, b, &r)) return false; break;
    case 'x':
        if (__builtin_mul_overflow(r, b, &t)) return false;
        r = t / CALC_SCALE;
        break;
    case '/':
        if (b == 0) return false;
        if (__builtin_mul_overflow(r, CALC_SCALE, &t)) return false;
        r = t / b;
        break;
    default: r = b; break;
    }
    c->acc = r;
    return true;
}

static void calc_format(const calc_t *c, char *out, size_t size) {
    if (c->error) { ksnprintf(out, size, "Error"); return; }
    if (c->typing) { ksnprintf(out, size, "%s", c->in); return; }
    int64_t v = c->cur;
    bool neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
    unsigned long long ip = (unsigned long long)(u / CALC_SCALE);
    unsigned fp = (unsigned)(u % CALC_SCALE);
    if (fp == 0) { ksnprintf(out, size, "%s%llu", neg ? "-" : "", ip); return; }
    char frac[5] = { (char)('0' + fp / 1000), (char)('0' + (fp / 100) % 10), (char)('0' + (fp / 10) % 10), (char)('0' + fp % 10), 0 };
    for (int i = 3; i > 0 && frac[i] == '0'; i--) frac[i] = '\0';
    ksnprintf(out, size, "%s%llu.%s", neg ? "-" : "", ip, frac);
}

static void calc_press(calc_t *c, char key) {
    if (c->error && key != 'C') return;

    if ((key >= '0' && key <= '9') || key == '.') {
        if (!c->typing) { c->typing = true; c->in[0] = '\0'; }
        size_t n = k_strlen(c->in);
        const char *dot = 0;
        for (size_t i = 0; i < n; i++) if (c->in[i] == '.') dot = &c->in[i];
        if (key == '.') {
            if (dot) return;
            if (n == 0) { c->in[n++] = '0'; }
            c->in[n++] = '.'; c->in[n] = '\0';
        } else {
            if (dot ? (int)(&c->in[n] - dot - 1) >= 4 : (n >= 9 && !(n == 1 && c->in[0] == '0'))) return;
            if (n == 1 && c->in[0] == '0' && !dot) n = 0;              /* replace a lone leading zero */
            c->in[n++] = key; c->in[n] = '\0';
        }
        c->cur = calc_parse(c->in);
        return;
    }

    switch (key) {
    case 'C': calc_reset(c); break;
    case '<':
        if (c->typing) {
            size_t n = k_strlen(c->in);
            if (n > 0) c->in[--n] = '\0';
            if (n == 0) { c->in[0] = '0'; c->in[1] = '\0'; }
            c->cur = calc_parse(c->in);
        }
        break;
    case '+': case '-': case 'x': case '/':
        if (c->op && c->typing) { if (!calc_compute(c)) { c->error = true; return; } }
        else if (!c->op) c->acc = c->cur;
        c->cur = c->acc; c->typing = false; c->op = key;
        break;
    case '=':
        if (c->op) { if (!calc_compute(c)) { c->error = true; return; } }
        else c->acc = c->cur;
        c->cur = c->acc; c->typing = false; c->op = 0;
        break;
    default: break;
    }
}

static rect_t calc_button_rect(rect_t c, int i) {
    int gx = c.x + (c.w - 300) / 2, gy = c.y + 15 + 48 + 8;
    if (i == 16) return R(gx, gy + 4 * 47, 2 * 72 + 5, 42);            /* wide "0" */
    if (i == 17) return R(gx + 2 * 77, gy + 4 * 47, 72, 42);          /* "." */
    return R(gx + (i % 4) * 77, gy + (i / 4) * 47, 72, 42);
}

static void calc_paint(gfx_t *g, window_t *w, rect_t c) {
    calc_t *st = (calc_t *)w->data;
    gfx_fill_rect(g, c.x, c.y, c.w, c.h, RGB(0xEA, 0xF3, 0xF9));
    int gx = c.x + (c.w - 300) / 2, gy = c.y + 15;
    gfx_fill_rect(g, gx, gy, 300, 48, RGB(255, 255, 255));
    gfx_stroke_round_rect(g, gx, gy, 300, 48, 0, RGB(0x99, 0x99, 0x99));
    char txt[40];
    calc_format(st, txt, sizeof txt);
    int tw = font_text_width(&font_ui24, txt);
    gfx_text(g, &font_ui24, gx + 292 - tw, gy + 33, txt, RGB(0x20, 0x20, 0x20));
    if (st->op) {
        char o[2] = { st->op, 0 };
        gfx_text(g, &font_ui13b, gx + 8, gy + 18, o, RGB(0x60, 0x70, 0x80));
    }
    for (int i = 0; i < CALC_BTNS; i++) {
        rect_t b = calc_button_rect(c, i);
        bool op = (i < 16) && (i % 4 == 3);
        paint_t p = (st->pressed == i)
            ? paint_v2(RGB(0xB8, 0xC8, 0xD4), RGB(0xD5, 0xDC, 0xE2))
            : (op ? paint_v2(RGB(0xFF, 0xF0, 0xDC), RGB(0xF3, 0xCF, 0xA0))
                  : paint_v2(RGB(0xFF, 0xFF, 0xFF), RGB(0xD5, 0xDC, 0xE2)));
        gfx_fill_round_paint(g, b.x, b.y, b.w, b.h, 3, &p);
        gfx_stroke_round_rect(g, b.x, b.y, b.w, b.h, 3, RGB(0x99, 0xAA, 0xAA));
        gfx_text_center(g, &font_ui13, b.x + b.w / 2, text_baseline_in(&font_ui13, b.y, b.h), calc_labels[i], COL_TEXT);
    }
}

static void calc_click(window_t *w, int lx, int ly, int buttons, int prev) {
    calc_t *st = (calc_t *)w->data;
    rect_t c = window_client_size(w);
    int x = lx, y = ly;
    bool down = (buttons & 1) && !(prev & 1);
    bool up = !(buttons & 1) && (prev & 1);
    if (down) {
        for (int i = 0; i < CALC_BTNS; i++)
            if (rect_contains(calc_button_rect(c, i), x, y)) { st->pressed = i; window_invalidate(w); return; }
    } else if (up && st->pressed >= 0) {
        int i = st->pressed;
        st->pressed = -1;
        if (rect_contains(calc_button_rect(c, i), x, y)) calc_press(st, calc_keys[i]);
        window_invalidate(w);
    }
}

static void calc_key(window_t *w, uint16_t key, uint8_t mods) {
    calc_t *st = (calc_t *)w->data;
    if (mods & (MOD_CTRL | MOD_ALT)) return;
    char k = 0;
    if (key >= '0' && key <= '9') k = (char)key;
    else if (key == '+' || key == '-' || key == '/' || key == '.') k = (char)key;
    else if (key == '*' || key == 'x' || key == 'X') k = 'x';
    else if (key == '\n' || key == '=') k = '=';
    else if (key == KEY_BACKSPACE) k = '<';
    else if (key == 'c' || key == 'C' || key == KEY_ESC) k = 'C';
    if (!k) return;
    calc_press(st, k);
    window_invalidate(w);
}

/* ════════════════════════════════════════════════════════════════════════════
   About (modal dialog)
   ════════════════════════════════════════════════════════════════════════════ */
static void about_paint(gfx_t *g, window_t *w, rect_t c) {
    (void)w;
    gfx_fill_rect(g, c.x, c.y, c.w, c.h, RGB(0xEA, 0xF3, 0xF9));
    icon_draw_tile(g, ICON_LAUNCHPAD, c.x + 24, c.y + 24, 64, 3);
    gfx_text(g, &font_ui24, c.x + 108, c.y + 52, "BornomalaOS", COL_TEXT);
    gfx_text(g, &font_ui13, c.x + 108, c.y + 76, "Phase 6 - desktop environment", COL_LINK);
    gfx_text(g, &font_ui12, c.x + 108, c.y + 100, "64-bit kernel, software compositor,", RGB(0x50, 0x60, 0x70));
    gfx_text(g, &font_ui12, c.x + 108, c.y + 116, "window manager and desktop shell.", RGB(0x50, 0x60, 0x70));
    rect_t ok = R(c.x + c.w - 100, c.y + c.h - 46, 80, 28);
    paint_t p = paint_v2(RGB(0xFF, 0xFF, 0xFF), RGB(0xD5, 0xDC, 0xE2));
    gfx_fill_round_paint(g, ok.x, ok.y, ok.w, ok.h, 3, &p);
    gfx_stroke_round_rect(g, ok.x, ok.y, ok.w, ok.h, 3, RGB(0x4C, 0x8B, 0xD0));
    gfx_text_center(g, &font_ui13, ok.x + ok.w / 2, text_baseline_in(&font_ui13, ok.y, ok.h), "OK", COL_TEXT);
}
static void about_mouse(window_t *w, int lx, int ly, int buttons, int prev) {
    rect_t c = window_client_size(w);
    if ((buttons & 1) && !(prev & 1) && rect_contains(R(c.w - 100, c.h - 46, 80, 28), lx, ly)) window_close(w);
}
static void about_key(window_t *w, uint16_t key, uint8_t mods) {
    (void)mods;
    if (key == '\n' || key == KEY_ESC) window_close(w);
}

/* ════════════════════════════════════════════════════════════════════════════
   launcher
   ════════════════════════════════════════════════════════════════════════════ */
window_t *apps_launch(app_id_t id, int sw, int sh, const char *username) {
    window_t *existing = wm_find_owner(id);
    if (existing) { window_focus(existing); return existing; }

    rect_t r; const char *title; icon_id_t icon; unsigned flags = WINDOW_MOVABLE; int minw = 300, minh = 200;
    switch (id) {
    case APP_COMPUTER: r = R(190, 80, 720, 455);  title = "Computer";   icon = ICON_COMPUTER; flags |= WINDOW_RESIZABLE; minw = 460; minh = 300; break;
    case APP_FILES:    r = R(235, 120, 690, 420); title = "Files";      icon = ICON_FILES;    flags |= WINDOW_RESIZABLE; minw = 460; minh = 300; break;
    case APP_NOTEPAD:  r = R(280, 135, 620, 390); title = "Notepad - Untitled"; icon = ICON_NOTEPAD; flags |= WINDOW_RESIZABLE; minw = 320; minh = 200; break;
    case APP_CALC:     r = R(350, 120, 340, 390); title = "Calculator"; icon = ICON_CALC;     minw = 340; minh = 390; break;
    case APP_ABOUT:    r = R((sw - 420) / 2, (sh - 240) / 2, 420, 240); title = "About BornomalaOS"; icon = ICON_LAUNCHPAD;
                       flags = WINDOW_MOVABLE | WINDOW_MODAL | WINDOW_NO_MINMAX; minw = 420; minh = 240; break;
    default: return 0;
    }
    if (id != APP_ABOUT) {                                   /* keep the window on small screens */
        if (r.x + r.w > sw - 10) r.x = sw - 10 - r.w;
        if (r.y + r.h > sh - 90) r.y = sh - 90 - r.h;
        if (r.x < 10) r.x = 10;
        if (r.y < 40) r.y = 40;
    }

    char names[64];
    if (id == APP_FILES && username && *username) { ksnprintf(names, sizeof names, "%s's Files", username); title = names; }

    window_t *w = window_create(id, title, icon, r, minw, minh, flags, 0);
    if (!w) { notify_post("BornomalaOS", "Not enough memory to open the window"); return 0; }

    switch (id) {
    case APP_COMPUTER: w->paint = computer_paint; break;
    case APP_FILES: {
        files_state_t *st = (files_state_t *)kcalloc(1, sizeof(*st));
        if (!st) { window_close(w); return 0; }
        st->selected = -1;
        w->data = st; w->paint = files_paint; w->on_mouse = files_mouse; w->on_close = free_data;
        break; }
    case APP_NOTEPAD: {
        notepad_t *n = (notepad_t *)kcalloc(1, sizeof(*n));
        if (!n) { window_close(w); return 0; }
        k_strncpy(n->buf, np_initial, NP_MAX);
        n->len = (int)k_strlen(n->buf);
        n->caret = n->len; n->want_col = -1; n->sel_anchor = -1; n->caret_on = true;
        w->data = n; w->paint = notepad_paint; w->on_key = notepad_key; w->on_mouse = notepad_mouse;
        w->on_command = notepad_command; w->on_close = free_data;
        break; }
    case APP_CALC: {
        calc_t *cs = (calc_t *)kcalloc(1, sizeof(*cs));
        if (!cs) { window_close(w); return 0; }
        calc_reset(cs);
        w->data = cs; w->paint = calc_paint; w->on_mouse = calc_click; w->on_key = calc_key; w->on_close = free_data;
        break; }
    case APP_ABOUT:
        w->paint = about_paint; w->on_mouse = about_mouse; w->on_key = about_key;
        break;
    default: break;
    }
    window_invalidate(w);
    return w;
}

void apps_tick(uint64_t ticks) {
    window_t *w = wm_find_owner(APP_NOTEPAD);
    if (!w || w->minimized || wm_active() != w) return;
    notepad_t *n = (notepad_t *)w->data;
    bool on = ((ticks / 50) & 1) == 0;                 /* 2 Hz blink at 100 Hz timer */
    if (on != n->caret_on) { n->caret_on = on; window_invalidate(w); }
}
