#include "users.h"
#include "klib.h"

static bm_user_store_t g_users;

static void clear_user(bm_user_t *u) { memset(u, 0, sizeof(*u)); }

/* Bootstrap password digest placeholder. Replace with Argon2id/scrypt + a random
   per-user salt before persistent accounts ship. */
static void bootstrap_hash(const char *password, uint8_t out[32]) {
    uint64_t h[4] = { 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
                      0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL };
    for (size_t i = 0; password && password[i]; ++i) {
        uint64_t c = (unsigned char)password[i];
        h[i & 3] ^= c + 0x9e3779b97f4a7c15ULL + (h[(i + 1) & 3] << 6) + (h[(i + 1) & 3] >> 2);
        h[i & 3] *= 0x100000001b3ULL;
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 8; j++) out[i * 8 + j] = (uint8_t)(h[i] >> (j * 8));
}

static bm_user_t *slot_by_id(uint32_t id) {
    /* Scan ALL slots: removing a user leaves a hole, it must not hide later users. */
    for (uint32_t i = 0; i < BM_MAX_USERS; i++)
        if (g_users.entries[i].active && g_users.entries[i].id == id) return &g_users.entries[i];
    return 0;
}

void users_init(void) {
    memset(&g_users, 0, sizeof(g_users));
    bm_user_t *u = &g_users.entries[0];
    u->id = 1; u->active = true; u->password_set = true; u->role = USER_ROLE_ADMIN;
    k_strcpy(u->username, "Mahfuz");
    k_strcpy(u->display_name, "Mahfuz");
    k_strcpy(u->home, "C:\\Users\\Mahfuz");
    /* Prototype password is "bornomala" (bootstrap build only). */
    bootstrap_hash("bornomala", u->password_hash);
    g_users.count = 1;
    g_users.current_user_id = u->id;
    g_users.next_id = 2;
}

const bm_user_store_t *users_store(void) { return &g_users; }
const bm_user_t *users_current(void) { return slot_by_id(g_users.current_user_id); }
const bm_user_t *users_find_by_id(uint32_t id) { return slot_by_id(id); }

const bm_user_t *users_find_by_name(const char *username) {
    if (!username) return 0;
    for (uint32_t i = 0; i < BM_MAX_USERS; i++)
        if (g_users.entries[i].active && k_strcmp(g_users.entries[i].username, username) == 0)
            return &g_users.entries[i];
    return 0;
}

bool users_add(const char *username, const char *display_name, user_role_t role, uint32_t *out_id) {
    if (!username || !*username || users_find_by_name(username)) return false;
    bm_user_t *slot = 0;
    for (uint32_t i = 0; i < BM_MAX_USERS; i++)
        if (!g_users.entries[i].active) { slot = &g_users.entries[i]; break; }
    if (!slot) return false;

    clear_user(slot);
    slot->id = g_users.next_id++;
    slot->active = true;
    slot->role = role;
    k_strncpy(slot->username, username, BM_USERNAME_MAX);
    k_strncpy(slot->display_name, (display_name && *display_name) ? display_name : username, BM_DISPLAY_NAME_MAX);
    /* Home path is derived and lives outside the protected system partition. */
    k_strcpy(slot->home, "C:\\Users\\");
    size_t n = k_strlen(slot->home);
    k_strncpy(slot->home + n, slot->username, BM_HOME_MAX - n);
    g_users.count++;
    if (out_id) *out_id = slot->id;
    return true;
}

bool users_update(uint32_t id, const char *display_name, user_role_t role) {
    bm_user_t *u = slot_by_id(id);
    if (!u) return false;
    if (display_name && *display_name) k_strncpy(u->display_name, display_name, BM_DISPLAY_NAME_MAX);
    u->role = role;
    return true;
}

bool users_remove(uint32_t id) {
    bm_user_t *u = slot_by_id(id);
    if (!u || id == g_users.current_user_id) return false;
    clear_user(u);
    if (g_users.count) g_users.count--;
    return true;
}

bool users_set_current(uint32_t id) {
    if (!slot_by_id(id)) return false;
    g_users.current_user_id = id;
    return true;
}

bool users_set_password(uint32_t id, const char *password) {
    bm_user_t *u = slot_by_id(id);
    if (!u || !password) return false;
    bootstrap_hash(password, u->password_hash);
    u->password_set = true;
    return true;
}

bool users_verify_password(uint32_t id, const char *password) {
    const bm_user_t *u = slot_by_id(id);
    if (!u || !u->password_set || !password) return false;
    uint8_t digest[32];
    bootstrap_hash(password, digest);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= (uint8_t)(digest[i] ^ u->password_hash[i]);
    return diff == 0;
}
