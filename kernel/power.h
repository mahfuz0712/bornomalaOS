/* kernel/power.h - restart / shut down */
#ifndef BORNOMALA_POWER_H
#define BORNOMALA_POWER_H

void power_restart(void) __attribute__((noreturn));
void power_shutdown(void) __attribute__((noreturn));

#endif
