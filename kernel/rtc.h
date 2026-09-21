/* kernel/rtc.h - CMOS real-time clock */
#ifndef BORNOMALA_RTC_H
#define BORNOMALA_RTC_H

#include <stdint.h>

typedef struct {
    uint8_t  second, minute, hour, day, month;
    uint16_t year;
    uint8_t  weekday;              /* 0 = Sunday */
} rtc_time_t;

void rtc_read(rtc_time_t *out);

/* Formatting helpers. Buffers: time >= 9, date_short >= 16, date_long >= 32 bytes. */
void rtc_format_time(const rtc_time_t *t, char *out);        /* "14:05:09" */
void rtc_format_hm(const rtc_time_t *t, char *out);          /* "14:05"    */
void rtc_format_date_short(const rtc_time_t *t, char *out);  /* "Sun, Sep 20"        */
void rtc_format_date_long(const rtc_time_t *t, char *out);   /* "Sunday, September 20" */

#endif
