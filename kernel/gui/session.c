#include "session.h"
#include "compositor.h"
#include "desktop.h"
#include "icons.h"
#include "lock.h"
#include "../cpu.h"
#include "../console.h"
#include "../input.h"
#include "../interrupts.h"

static scene_id_t scene = SCENE_LOCK;
static scene_id_t pending_scene = SCENE_LOCK;
static bool       switch_pending;
static int        mx, my, prev_buttons;

int session_mouse_x(void) { return mx; }
int session_mouse_y(void) { return my; }

void session_switch(scene_id_t s) {
    pending_scene = s;
    switch_pending = true;
}

static rect_t cursor_damage_rect(void) { return R(mx - 3, my - 3, CURSOR_W + 8, CURSOR_H + 8); }

static void apply_scene_switch(void) {
    switch_pending = false;
    scene = pending_scene;
    prev_buttons = 0;
    switch (scene) {
    case SCENE_LOCK:    lock_enter(); break;
    case SCENE_DESKTOP: desktop_enter(); break;
    case SCENE_SLEEP:   compositor_blank(); break;
    }
    compositor_damage_all();
}

static void handle_event(const input_event_t *ev) {
    if (scene == SCENE_SLEEP) {
        /* any key press, click or mouse movement wakes the machine back to the lock screen */
        if (ev->type == INPUT_KEY || ev->buttons || ev->dx || ev->dy) session_switch(SCENE_LOCK);
        return;
    }

    if (ev->type == INPUT_KEY) {
        if (scene == SCENE_LOCK) lock_key(ev->key, ev->mods);
        else                     desktop_key(ev->key, ev->mods);
        return;
    }

    if (ev->type == INPUT_MOUSE) {
        compositor_add_damage(cursor_damage_rect());
        mx += ev->dx; my += ev->dy;
        if (mx < 0) mx = 0;
        if (my < 0) my = 0;
        if (mx >= compositor_width())  mx = compositor_width() - 1;
        if (my >= compositor_height()) my = compositor_height() - 1;
        compositor_add_damage(cursor_damage_rect());

        int buttons = ev->buttons, prev = prev_buttons;
        prev_buttons = buttons;
        if (scene == SCENE_LOCK) lock_mouse(mx, my, buttons, prev);
        else                     desktop_mouse(mx, my, buttons, prev);
    }
}

static void render(void) {
    gfx_t *g = compositor_begin_frame();
    if (scene == SCENE_LOCK) lock_draw(g);
    else if (scene == SCENE_DESKTOP) desktop_draw(g);
    cursor_draw(g, mx, my);
    compositor_end_frame();
}

bool session_run(const mb2_framebuffer_t *fb) {
    if (!compositor_init(fb)) return false;
    kprintf("gui: %ux%u framebuffer at %p, pitch %u\n", fb->width, fb->height, (void *)(uintptr_t)fb->address, fb->pitch);

    mx = compositor_width() / 2;
    my = compositor_height() / 2;
    scene = pending_scene = SCENE_LOCK;
    lock_enter();
    compositor_add_damage(cursor_damage_rect());

    for (;;) {
        if (switch_pending) apply_scene_switch();

        input_event_t ev;
        while (input_pop(&ev)) {
            handle_event(&ev);
            if (switch_pending) break;                 /* re-enter the loop so the new scene is set up first */
        }
        if (switch_pending) continue;

        uint64_t now = timer_ticks();
        if (scene == SCENE_LOCK) lock_tick(now);
        else if (scene == SCENE_DESKTOP) desktop_tick(now);
        if (switch_pending) continue;

        if (scene != SCENE_SLEEP && compositor_has_damage()) render();

        /* Sleep until the next interrupt (timer at 100 Hz, or input) unless work already arrived. */
        cpu_cli();
        if (!input_pending() && !switch_pending && !compositor_has_damage()) cpu_idle();
        else cpu_sti();
    }
}
