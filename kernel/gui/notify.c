#include "notify.h"
#include "compositor.h"
#include "../interrupts.h"
#include "../klib.h"

#define MAX_NOTES 4
#define NOTE_W 330
#define NOTE_H 62
#define NOTE_LIFE 450          /* 4.5 s at 100 Hz */

typedef struct { bool used; char title[40], body[96]; uint64_t until; } note_t;
static note_t notes[MAX_NOTES];

static rect_t note_rect(int slot) { return R(compositor_width() - 15 - NOTE_W, 42 + slot * (NOTE_H + 8), NOTE_W, NOTE_H); }
static rect_t all_rect(void) { return R(compositor_width() - 30 - NOTE_W, 30, NOTE_W + 40, MAX_NOTES * (NOTE_H + 8) + 30); }

void notify_post(const char *title, const char *body) {
    int slot = -1;
    for (int i = 0; i < MAX_NOTES; i++) if (!notes[i].used) { slot = i; break; }
    if (slot < 0) {                                   /* full: drop the oldest */
        for (int i = 1; i < MAX_NOTES; i++) notes[i - 1] = notes[i];
        slot = MAX_NOTES - 1;
    }
    note_t *n = &notes[slot];
    memset(n, 0, sizeof(*n));
    n->used = true;
    k_strncpy(n->title, title, sizeof(n->title) - 1);
    k_strncpy(n->body, body, sizeof(n->body) - 1);
    n->until = timer_ticks() + NOTE_LIFE;
    compositor_add_damage(all_rect());
}

void notify_reset(void) { memset(notes, 0, sizeof notes); compositor_add_damage(all_rect()); }

void notify_tick(uint64_t ticks) {
    bool changed = false;
    for (int i = 0; i < MAX_NOTES; i++)
        if (notes[i].used && ticks >= notes[i].until) { notes[i].used = false; changed = true; }
    if (!changed) return;
    int w = 0;                                         /* compact so banners keep stacking from the top */
    for (int i = 0; i < MAX_NOTES; i++) if (notes[i].used) notes[w++] = notes[i];
    for (int i = w; i < MAX_NOTES; i++) memset(&notes[i], 0, sizeof(note_t));
    compositor_add_damage(all_rect());
}

void notify_draw(gfx_t *g) {
    if (!gfx_visible(g, all_rect())) return;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (!notes[i].used) continue;
        rect_t r = note_rect(i);
        gfx_shadow(g, r.x, r.y, r.w, r.h, 10, 12, 40, 0, 4);
        gfx_fill_round_rect(g, r.x, r.y, r.w, r.h, 10, RGBA(20, 40, 55, 93));
        gfx_stroke_round_rect(g, r.x, r.y, r.w, r.h, 10, RGBA(145, 200, 232, 70));
        gfx_fill_round_rect(g, r.x + 12, r.y + 14, 4, r.h - 28, 2, RGB(0x5B, 0xB4, 0xFF));
        gfx_text(g, &font_ui13b, r.x + 26, r.y + 24, notes[i].title, RGB(255, 255, 255));
        gfx_text(g, &font_ui12, r.x + 26, r.y + 44, notes[i].body, WHITE_A(80));
    }
}
