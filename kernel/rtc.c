#include "rtc.h"
#include "cpu.h"
#include "console.h"
#include <stdbool.h>

static uint8_t cmos(uint8_t reg) { outb(0x70, reg); io_wait(); return inb(0x71); }
static uint8_t bcd2bin(uint8_t x) { return (uint8_t)((x & 0x0F) + (x >> 4) * 10); }

static const char *const month_short[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
static const char *const month_long[]  = { "January","February","March","April","May","June","July","August","September","October","November","December" };
static const char *const day_short[]   = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
static const char *const day_long[]    = { "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday" };

typedef struct { uint8_t s, m, h, d, mo, y; } raw_t;

static void read_raw(raw_t *r) {
    for (int i = 0; i < 100000 && (cmos(0x0A) & 0x80); i++) {}   /* wait out an update in progress */
    r->s = cmos(0x00); r->m = cmos(0x02); r->h = cmos(0x04);
    r->d = cmos(0x07); r->mo = cmos(0x08); r->y = cmos(0x09);
}

static int weekday_of(int y, int m, int d) {                /* Sakamoto: 0 = Sunday */
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 1 || m > 12) return 0;
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

void rtc_read(rtc_time_t *o) {
    if (!o) return;
    raw_t a, b;
    read_raw(&a);
    for (int i = 0; i < 5; i++) {                            /* re-read until two reads agree */
        read_raw(&b);
        if (a.s == b.s && a.m == b.m && a.h == b.h && a.d == b.d && a.mo == b.mo && a.y == b.y) break;
        a = b;
    }
    uint8_t status_b = cmos(0x0B);
    bool pm = (a.h & 0x80) != 0;                             /* PM flag lives in bit 7 in 12-hour mode */
    a.h &= 0x7F;                                             /* ...so strip it BEFORE any BCD decoding */
    if (!(status_b & 0x04)) {                                /* BCD mode */
        a.s = bcd2bin(a.s); a.m = bcd2bin(a.m); a.h = bcd2bin(a.h);
        a.d = bcd2bin(a.d); a.mo = bcd2bin(a.mo); a.y = bcd2bin(a.y);
    }
    if (!(status_b & 0x02)) {                                /* 12-hour clock -> 24-hour */
        if (a.h == 12) a.h = 0;
        if (pm) a.h = (uint8_t)(a.h + 12);
    }
    o->second = a.s; o->minute = a.m; o->hour = (uint8_t)(a.h % 24);
    o->day = a.d ? a.d : 1; o->month = (a.mo >= 1 && a.mo <= 12) ? a.mo : 1;
    o->year = (uint16_t)(2000 + a.y);
    o->weekday = (uint8_t)weekday_of(o->year, o->month, o->day);
}

static void two(char *p, unsigned v) { p[0] = (char)('0' + (v / 10) % 10); p[1] = (char)('0' + v % 10); }

void rtc_format_time(const rtc_time_t *t, char *out) {
    two(out, t->hour); out[2] = ':'; two(out + 3, t->minute); out[5] = ':'; two(out + 6, t->second); out[8] = '\0';
}

void rtc_format_hm(const rtc_time_t *t, char *out) {
    two(out, t->hour); out[2] = ':'; two(out + 3, t->minute); out[5] = '\0';
}

void rtc_format_date_short(const rtc_time_t *t, char *out) {
    ksnprintf(out, 16, "%s, %s %u", day_short[t->weekday % 7], month_short[(t->month - 1) % 12], (unsigned)t->day);
}

void rtc_format_date_long(const rtc_time_t *t, char *out) {
    ksnprintf(out, 32, "%s, %s %u", day_long[t->weekday % 7], month_long[(t->month - 1) % 12], (unsigned)t->day);
}
