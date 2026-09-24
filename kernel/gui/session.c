<<<<<<< HEAD
#include "session.h"
#include "compositor.h"
#include "desktop.h"
#include "icons.h"
#include "lock.h"
#include "sysscreen.h"
#include "uistate.h"
#include "wm.h"
#include "../cpu.h"
#include "../console.h"
#include "../input.h"
#include "../interrupts.h"
#include "../power.h"
#include "../users.h"
#include "notify.h"

static int mx, my, prev_buttons;
static bool persistent_notice_pending;

int session_mouse_x(void) { return mx; }
int session_mouse_y(void) { return my; }

static rect_t cursor_damage_rect(void) { return R(mx - 3, my - 3, CURSOR_W + 8, CURSOR_H + 8); }

/* Runs a sysscreen animation to completion, rendering real frames (not a single static one),
   still servicing the timer so timer_ticks() advances and the fill/halo animate smoothly. */
static void run_sysscreen(sysscreen_kind_t kind) {
    const bm_user_t *u = users_current();
    char initial[2] = { (char)(u && u->display_name[0] ? u->display_name[0] : 'U'), 0 };
    sysscreen_enter(kind, initial);
    for (;;) {
        uint64_t now = timer_ticks();
        compositor_damage_all();
        gfx_t *g = compositor_begin_frame();
        sysscreen_draw(g, now);
        compositor_end_frame();
        if (sysscreen_progress_done(now)) break;
        cpu_cli();
        cpu_idle();                               /* wakes on the next 100 Hz tick */
    }
}

/* Applies a requested UI state change. Scenes are only re-entered when the scene really changes. */
static void apply_state(ui_state_t next) {
    ui_state_t old = ui_state();
    if (next == old) return;
    bool was_desktop = ui_is_desktop_scene(old), is_desktop = ui_is_desktop_scene(next);
    ui_commit(next);
    prev_buttons = 0;

    switch (next) {
    case UI_LOCK_SCREEN: lock_enter(); break;
    case UI_DESKTOP:
    case UI_LAUNCHPAD:
        if (!was_desktop) {
            desktop_enter();
            if (persistent_notice_pending) {
                notify_post("Persistent mode", "Install-to-disk is not available in this build yet.");
                persistent_notice_pending = false;
            }
        } else if (is_desktop) desktop_state_changed();
        break;
    case UI_SLEEP:
        compositor_blank();
        break;
    case UI_SIGNOUT:
        run_sysscreen(SYS_SIGNOUT);
        wm_reset();                               /* close every window: the next user starts clean */
        ui_commit(UI_LOCK_SCREEN);
        lock_enter();
        break;
    case UI_SHUTDOWN:
        run_sysscreen(SYS_SHUTDOWN);
        power_shutdown();                         /* never returns */
        break;
    case UI_RESTART:
        run_sysscreen(SYS_RESTART);
        power_restart();                          /* never returns */
        break;
    }
    compositor_damage_all();
    compositor_add_damage(cursor_damage_rect());
}

static void handle_event(const input_event_t *ev) {
    ui_state_t st = ui_state();
    if (st == UI_SLEEP) {
        if (ev->type == INPUT_KEY || ev->buttons || ev->dx || ev->dy) ui_request(UI_LOCK_SCREEN);
        return;
    }

    if (ev->type == INPUT_KEY) {
        if (st == UI_LOCK_SCREEN) lock_key(ev->key, ev->mods);
        else                      desktop_key(ev->key, ev->mods);
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
        if (st == UI_LOCK_SCREEN) lock_mouse(mx, my, buttons, prev);
        else                      desktop_mouse(mx, my, buttons, prev);
    }
}

static void render(ui_state_t st) {
    gfx_t *g = compositor_begin_frame();
    if (st == UI_LOCK_SCREEN) lock_draw(g);
    else if (ui_is_desktop_scene(st)) desktop_draw(g);
    cursor_draw(g, mx, my);
    compositor_end_frame();
}

/* Static info screen that waits for the person to press a key or click, then falls
   back to the Live Desktop. Used for boot_mode=installer until Phase 11 ships a real
   graphical installer -- we never pretend to install anything we cannot yet do. */
static void run_installer_placeholder(void) {
    sysscreen_enter(SYS_INSTALLER_INFO, 0);
    for (;;) {
        uint64_t now = timer_ticks();
        compositor_damage_all();
        gfx_t *g = compositor_begin_frame();
        sysscreen_draw(g, now);
        compositor_end_frame();

        input_event_t ev;
        bool go = false;
        while (input_pop(&ev)) {
            if (ev.type == INPUT_KEY) go = true;
            if (ev.type == INPUT_MOUSE && (ev.buttons & MOUSE_LEFT)) go = true;
        }
        if (go) return;
        cpu_cli();
        cpu_idle();
    }
}

bool session_run(const mb2_framebuffer_t *fb, boot_mode_t mode) {
    if (!compositor_init(fb)) return false;
    kprintf("gui: %ux%u framebuffer at %p, pitch %u\n", fb->width, fb->height, (void *)(uintptr_t)fb->address, fb->pitch);

    mx = compositor_width() / 2;
    my = compositor_height() / 2;

    run_sysscreen(SYS_BOOT);
    if (mode == BOOT_MODE_INSTALLER) run_installer_placeholder();
    persistent_notice_pending = (mode == BOOT_MODE_PERSISTENT);

    ui_commit(UI_LOCK_SCREEN);
    lock_enter();
    compositor_add_damage(cursor_damage_rect());

    for (;;) {
        ui_state_t next;
        if (ui_take_request(&next)) apply_state(next);

        input_event_t ev;
        while (input_pop(&ev)) {
            handle_event(&ev);
            ui_state_t peek;
            if (ui_take_request(&peek)) { apply_state(peek); break; }
        }

        ui_state_t st = ui_state();
        uint64_t now = timer_ticks();
        if (st == UI_LOCK_SCREEN) lock_tick(now);
        else if (ui_is_desktop_scene(st)) desktop_tick(now);
        if (ui_take_request(&next)) apply_state(next);

        st = ui_state();
        if (st != UI_SLEEP && compositor_has_damage()) render(st);

        cpu_cli();
        if (!input_pending() && !compositor_has_damage()) cpu_idle();
        else cpu_sti();
    }
}
=======
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
>>>>>>> 23b11cf3087acc2108f276bdfb25d3a6f909e2f7
