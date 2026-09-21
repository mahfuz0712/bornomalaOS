#include "input.h"
#include "cpu.h"

#define QUEUE_SIZE 256u   /* power of two */

static input_event_t queue[QUEUE_SIZE];
static volatile uint32_t head, tail;

void input_push(const input_event_t *ev) {
    uint32_t next = (head + 1) & (QUEUE_SIZE - 1);
    if (next == tail) return;                     /* full: drop the newest event */
    queue[head] = *ev;
    head = next;
}

bool input_pop(input_event_t *out) {
    uint64_t fl = irq_save();
    bool ok = (tail != head);
    if (ok) {
        *out = queue[tail];
        tail = (tail + 1) & (QUEUE_SIZE - 1);
    }
    irq_restore(fl);
    return ok;
}

bool input_pending(void) { return tail != head; }

void input_flush(void) {
    uint64_t fl = irq_save();
    tail = head;
    irq_restore(fl);
}
