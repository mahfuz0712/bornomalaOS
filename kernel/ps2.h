/* kernel/ps2.h - i8042 PS/2 controller shared by keyboard and mouse */
#ifndef BORNOMALA_PS2_H
#define BORNOMALA_PS2_H

#include <stdint.h>
#include <stdbool.h>

void ps2_init(void);                       /* configure controller, enable both ports + IRQs */
void ps2_irq_service(void);                /* call from IRQ1 and IRQ12 */

bool ps2_write_kbd(uint8_t byte);          /* send to keyboard, wait for ACK */
bool ps2_write_aux(uint8_t byte);          /* send to mouse,    wait for ACK */

#endif
