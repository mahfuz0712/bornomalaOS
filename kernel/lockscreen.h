#ifndef BORNOMALA_LOCKSCREEN_H
#define BORNOMALA_LOCKSCREEN_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LOCKSCREEN_LOCKED = 0,
    LOCKSCREEN_AUTHENTICATED = 1
} lockscreen_state_t;

void lockscreen_init(void);
lockscreen_state_t lockscreen_state(void);
bool lockscreen_try_unlock(const char *password);
void lockscreen_lock(void);
uint32_t lockscreen_user_id(void);

#endif
