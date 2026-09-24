#include "gpt.h"
#include "crc32.h"
#include "../alloc.h"
#include "../console.h"
#include "../klib.h"

const guid_t GUID_EFI_SYSTEM = { { 0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } };
const guid_t GUID_BASIC_DATA = { { 0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } };
/* BornomalaOS system partition: 547b9711-1f11-4a68-be6b-b1aaa4e3e0c0 */
const guid_t GUID_BORNOMALA_SYSTEM = { { 0x11, 0x97, 0x7B, 0x54, 0x11, 0x1F, 0x68, 0x4A, 0xBE, 0x6B, 0xB1, 0xAA, 0xA4, 0xE3, 0xE0, 0xC0 } };

typedef struct __attribute__((packed)) {
    char     sig[8];
    uint32_t revision, header_size, header_crc, reserved;
    uint64_t my_lba, alt_lba, first_usable, last_usable;
    uint8_t  disk_guid[16];
    uint64_t entries_lba;
    uint32_t num_entries, entry_size, entries_crc;
} gpt_header_raw_t;

typedef struct __attribute__((packed)) {
    uint8_t  type[16], unique[16];
    uint64_t first, last, attrs;
    uint16_t name[36];
} gpt_entry_raw_t;

bool guid_equal(guid_t a, guid_t b) { return memcmp(a.b, b.b, 16) == 0; }

static bool guid_zero(guid_t g) { for (int i = 0; i < 16; i++) if (g.b[i]) return false; return true; }

void guid_to_string(guid_t g, char *o) {
    const uint8_t *b = g.b;
    ksnprintf(o, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
              b[3], b[2], b[1], b[0], b[5], b[4], b[7], b[6], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

const char *gpt_type_name(guid_t t) {
    if (guid_equal(t, GUID_EFI_SYSTEM)) return "EFI System";
    if (guid_equal(t, GUID_BORNOMALA_SYSTEM)) return "BornomalaOS System";
    if (guid_equal(t, GUID_BASIC_DATA)) return "Basic Data";
    return "Unknown";
}

static void set_err(gpt_disk_t *g, const char *m) { k_strncpy(g->error, m, sizeof g->error - 1); }

static uint32_t entries_sectors(uint32_t n, uint32_t esize, uint32_t ssize) { return (n * esize + ssize - 1) / ssize; }

/* Parse one header (at `lba`) into g. Returns 0 or -1 with g->error set. */
static int parse_header(gpt_disk_t *g, uint64_t lba, uint64_t expect_my) {
    block_dev_t *d = g->dev;
    uint32_t ss = d->sector_size;
    uint8_t *sec = (uint8_t *)kmalloc(ss);
    if (!sec) { set_err(g, "out of memory"); return -1; }
    int rc = -1;
    uint8_t *ent = 0;

    if (block_read(d, lba, 1, sec)) { set_err(g, "cannot read GPT header"); goto out; }
    gpt_header_raw_t h;
    memcpy(&h, sec, sizeof h);
    if (memcmp(h.sig, "EFI PART", 8) != 0) { set_err(g, "no GPT signature"); goto out; }
    if (h.header_size < 92 || h.header_size > ss) { set_err(g, "bad header size"); goto out; }
    uint32_t saved = h.header_crc;
    ((gpt_header_raw_t *)sec)->header_crc = 0;
    if (crc32(sec, h.header_size) != saved) { set_err(g, "header CRC mismatch"); goto out; }
    if (h.my_lba != expect_my) { set_err(g, "header LBA mismatch"); goto out; }
    if (h.first_usable > h.last_usable || h.last_usable >= d->sector_count) { set_err(g, "usable range outside disk"); goto out; }
    if (h.entry_size < 128 || (h.entry_size & 7) || h.entry_size > 4096 || h.num_entries == 0 || h.num_entries > GPT_MAX_ENTRIES) {
        set_err(g, "unsupported entry table geometry"); goto out;
    }
    uint32_t esec = entries_sectors(h.num_entries, h.entry_size, ss);
    if (h.entries_lba + esec > d->sector_count) { set_err(g, "entry table outside disk"); goto out; }

    ent = (uint8_t *)kmalloc((size_t)esec * ss);
    if (!ent) { set_err(g, "out of memory"); goto out; }
    if (block_read(d, h.entries_lba, esec, ent)) { set_err(g, "cannot read entry table"); goto out; }
    if (crc32(ent, (size_t)h.num_entries * h.entry_size) != h.entries_crc) { set_err(g, "entry table CRC mismatch"); goto out; }

    memset(g->entries, 0, sizeof g->entries);
    memcpy(g->disk_guid.b, h.disk_guid, 16);
    g->first_usable = h.first_usable; g->last_usable = h.last_usable;
    g->alt_lba = h.alt_lba; g->num_entries = h.num_entries;
    for (uint32_t i = 0; i < h.num_entries; i++) {
        gpt_entry_raw_t e;
        memcpy(&e, ent + (size_t)i * h.entry_size, sizeof e);
        gpt_entry_t *o = &g->entries[i];
        memcpy(o->type.b, e.type, 16);
        if (guid_zero(o->type)) continue;                          /* unused slot */
        memcpy(o->unique.b, e.unique, 16);
        o->first_lba = e.first; o->last_lba = e.last; o->attrs = e.attrs;
        for (int k = 0; k < 36; k++) { uint16_t c = e.name[k]; o->name[k] = c == 0 ? 0 : (c < 128 ? (char)c : '?'); if (!c) break; }
        o->name[36] = '\0';
        o->used = true;
        if (o->first_lba > o->last_lba || o->first_lba < h.first_usable || o->last_lba > h.last_usable) {
            set_err(g, "partition outside usable range"); goto out;
        }
    }
    for (uint32_t i = 0; i < h.num_entries; i++) {                 /* overlap check */
        if (!g->entries[i].used) continue;
        for (uint32_t j = i + 1; j < h.num_entries; j++) {
            if (!g->entries[j].used) continue;
            if (g->entries[i].first_lba <= g->entries[j].last_lba && g->entries[j].first_lba <= g->entries[i].last_lba) {
                set_err(g, "overlapping partitions"); goto out;
            }
        }
    }
    rc = 0;
out:
    if (ent) kfree(ent);
    kfree(sec);
    return rc;
}

int gpt_read(block_dev_t *dev, gpt_disk_t *g) {
    memset(g, 0, sizeof *g);
    g->dev = dev;
    if (parse_header(g, 1, 1) == 0) { g->valid = true; return 0; }
    char first_err[80];
    k_strncpy(first_err, g->error, sizeof first_err - 1);
    first_err[sizeof first_err - 1] = '\0';
    if (dev->sector_count > 2 && parse_header(g, dev->sector_count - 1, dev->sector_count - 1) == 0) {
        g->valid = true; g->used_backup = true;                    /* recovered from the backup copy */
        return 0;
    }
    set_err(g, first_err);
    return -1;
}

/* ── writing ── */
static int write_one(gpt_disk_t *g, bool primary, const uint8_t *entries, uint32_t esec, uint32_t crc_entries) {
    block_dev_t *d = g->dev;
    uint32_t ss = d->sector_size;
    uint64_t last = d->sector_count - 1;
    uint64_t hdr_lba = primary ? 1 : last;
    uint64_t ent_lba = primary ? 2 : last - esec;

    uint8_t *sec = (uint8_t *)kcalloc(1, ss);
    if (!sec) return -1;
    gpt_header_raw_t *h = (gpt_header_raw_t *)sec;
    memcpy(h->sig, "EFI PART", 8);
    h->revision = 0x00010000; h->header_size = 92;
    h->my_lba = hdr_lba; h->alt_lba = primary ? last : 1;
    h->first_usable = g->first_usable; h->last_usable = g->last_usable;
    memcpy(h->disk_guid, g->disk_guid.b, 16);
    h->entries_lba = ent_lba; h->num_entries = g->num_entries; h->entry_size = 128; h->entries_crc = crc_entries;
    h->header_crc = crc32(sec, 92);
    int r = block_write(d, ent_lba, esec, entries);
    if (!r) r = block_write(d, hdr_lba, 1, sec);
    kfree(sec);
    return r;
}

int gpt_write(gpt_disk_t *g) {
    block_dev_t *d = g->dev;
    if (!g->valid || d->read_only) return -1;
    uint32_t ss = d->sector_size, esec = entries_sectors(g->num_entries, 128, ss);
    uint8_t *ent = (uint8_t *)kcalloc(esec, ss);
    if (!ent) return -1;
    for (uint32_t i = 0; i < g->num_entries; i++) {
        const gpt_entry_t *e = &g->entries[i];
        if (!e->used) continue;
        gpt_entry_raw_t raw;
        memset(&raw, 0, sizeof raw);
        memcpy(raw.type, e->type.b, 16); memcpy(raw.unique, e->unique.b, 16);
        raw.first = e->first_lba; raw.last = e->last_lba; raw.attrs = e->attrs;
        for (int k = 0; k < 36 && e->name[k]; k++) raw.name[k] = (uint8_t)e->name[k];
        memcpy(ent + (size_t)i * 128, &raw, sizeof raw);
    }
    uint32_t crc = crc32(ent, (size_t)g->num_entries * 128);
    int r = write_one(g, false, ent, esec, crc);                    /* backup first: a crash keeps one good copy */
    if (!r) r = write_one(g, true, ent, esec, crc);
    if (!r) r = block_flush(d);
    kfree(ent);
    return r;
}

int gpt_init_blank(block_dev_t *d, gpt_disk_t *g) {
    if (d->read_only || d->sector_count < 68) return -1;
    memset(g, 0, sizeof *g);
    g->dev = d; g->valid = true; g->num_entries = GPT_MAX_ENTRIES;
    uint32_t esec = entries_sectors(GPT_MAX_ENTRIES, 128, d->sector_size);
    g->first_usable = 2 + esec;
    g->last_usable = d->sector_count - 1 - esec - 1;
    g->alt_lba = d->sector_count - 1;
    /* pseudo-random disk GUID from the tick counter + capacity (real entropy arrives with the security phase) */
    uint64_t seed = d->sector_count * 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 16; i++) { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; g->disk_guid.b[i] = (uint8_t)(seed >> 56); }
    g->disk_guid.b[7] = (uint8_t)((g->disk_guid.b[7] & 0x0F) | 0x40);
    g->disk_guid.b[8] = (uint8_t)((g->disk_guid.b[8] & 0x3F) | 0x80);

    uint8_t *mbr = (uint8_t *)kcalloc(1, d->sector_size);       /* protective MBR */
    if (!mbr) return -1;
    mbr[446 + 2] = 0x02; mbr[446 + 4] = 0xEE;
    mbr[446 + 8] = 1;
    uint64_t span = d->sector_count - 1 > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : d->sector_count - 1;
    memcpy(mbr + 446 + 12, &span, 4);
    mbr[510] = 0x55; mbr[511] = 0xAA;
    int r = block_write(d, 0, 1, mbr);
    kfree(mbr);
    return r ? r : gpt_write(g);
}

bool gpt_find_free(const gpt_disk_t *g, uint64_t *first, uint64_t *last) {
    uint64_t best_a = 0, best_b = 0, cursor = g->first_usable;
    for (;;) {
        /* next used partition at or after cursor */
        uint64_t next_start = g->last_usable + 1;
        uint64_t next_end = 0;
        for (uint32_t i = 0; i < g->num_entries; i++) {
            const gpt_entry_t *e = &g->entries[i];
            if (e->used && e->first_lba >= cursor && e->first_lba < next_start) { next_start = e->first_lba; next_end = e->last_lba; }
        }
        if (next_start > cursor && next_start - cursor > best_b - best_a + (best_b ? 1 : 0) - (best_b ? 0 : 0)) {
            if (!best_b || next_start - cursor > best_b - best_a + 1) { best_a = cursor; best_b = next_start - 1; }
        }
        if (next_start > g->last_usable) break;
        cursor = next_end + 1;
    }
    if (!best_b) return false;
    *first = best_a; *last = best_b;
    return true;
}

int gpt_create_partition(gpt_disk_t *g, guid_t type, uint64_t first, uint64_t last, const char *name) {
    if (!g->valid || first < g->first_usable || last > g->last_usable || first > last) return -1;
    for (uint32_t i = 0; i < g->num_entries; i++)
        if (g->entries[i].used && first <= g->entries[i].last_lba && g->entries[i].first_lba <= last) return -2;   /* overlap */
    for (uint32_t i = 0; i < g->num_entries; i++) {
        if (g->entries[i].used) continue;
        gpt_entry_t *e = &g->entries[i];
        memset(e, 0, sizeof *e);
        e->used = true; e->type = type; e->first_lba = first; e->last_lba = last;
        k_strncpy(e->name, name, 36);
        uint64_t seed = first * 0x9E3779B97F4A7C15ULL ^ last ^ ((uint64_t)i << 40);
        for (int k = 0; k < 16; k++) { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; e->unique.b[k] = (uint8_t)(seed >> 56); }
        return (int)i;
    }
    return -3;                                                      /* table full */
}

int gpt_delete_partition(gpt_disk_t *g, int index) {
    if (index < 0 || (uint32_t)index >= g->num_entries || !g->entries[index].used) return -1;
    memset(&g->entries[index], 0, sizeof(gpt_entry_t));
    return 0;
}

int gpt_rename_partition(gpt_disk_t *g, int index, const char *name) {
    if (index < 0 || (uint32_t)index >= g->num_entries || !g->entries[index].used) return -1;
    memset(g->entries[index].name, 0, sizeof g->entries[index].name);
    k_strncpy(g->entries[index].name, name, 36);
    return 0;
}

int gpt_register_partitions(gpt_disk_t *g) {
    int n = 0;
    for (uint32_t i = 0; i < g->num_entries; i++) {
        const gpt_entry_t *e = &g->entries[i];
        if (!e->used) continue;
        char nm[16];
        ksnprintf(nm, sizeof nm, "%sp%u", g->dev->name, (unsigned)(i + 1));
        if (block_create_partition(g->dev, nm, e->first_lba, e->last_lba - e->first_lba + 1)) n++;
    }
    return n;
}
