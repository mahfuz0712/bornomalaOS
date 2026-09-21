#ifndef BORNOMALA_USERS_H
#define BORNOMALA_USERS_H
#include <stdint.h>
#include <stdbool.h>

#define BM_MAX_USERS 32
#define BM_USERNAME_MAX 31
#define BM_DISPLAY_NAME_MAX 63
#define BM_HOME_MAX 127

typedef enum {
    USER_ROLE_STANDARD = 0,
    USER_ROLE_ADMIN = 1
} user_role_t;

typedef struct {
    uint32_t id;
    bool active;
    bool password_set;
    user_role_t role;
    char username[BM_USERNAME_MAX + 1];
    char display_name[BM_DISPLAY_NAME_MAX + 1];
    char home[BM_HOME_MAX + 1];
    uint8_t password_hash[32]; /* bootstrap digest; a memory-hard KDF replaces it later */
} bm_user_t;

typedef struct {
    bm_user_t entries[BM_MAX_USERS];   /* slots; an entry is valid only while .active */
    uint32_t count;                    /* number of ACTIVE users */
    uint32_t current_user_id;
    uint32_t next_id;                  /* ids are never reused */
} bm_user_store_t;

void users_init(void);
const bm_user_store_t *users_store(void);
const bm_user_t *users_current(void);
const bm_user_t *users_find_by_id(uint32_t id);
const bm_user_t *users_find_by_name(const char *username);

bool users_add(const char *username, const char *display_name, user_role_t role, uint32_t *out_id);
bool users_update(uint32_t id, const char *display_name, user_role_t role);
bool users_remove(uint32_t id); /* the logged-in user is always protected */
bool users_set_current(uint32_t id);
bool users_set_password(uint32_t id, const char *password);
bool users_verify_password(uint32_t id, const char *password);

#endif
