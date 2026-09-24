#ifndef BORNOMALA_CRC32_H
#define BORNOMALA_CRC32_H
#include <stdint.h>
#include <stddef.h>
uint32_t crc32(const void *data, size_t len);      /* IEEE 802.3, as used by GPT */
#endif
