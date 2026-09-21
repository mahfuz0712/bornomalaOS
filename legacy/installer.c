/* kernel/installer.c — BornomalaOS installer
   Works with NVMe, SATA SSD, and HDD via the disk abstraction layer */

#include "kernel.h"
#include "disk.h"
#include "ext4.h"
#include "alloc.h"

/* ── GPT ─────────────────────────────────────────────────────────── */
#define GPT_SIG 0x5452415020494645ULL

static const uint8_t GPT_EFI[16]={
    0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,
    0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B
};
static const uint8_t GPT_LINUX[16]={
    0xAF,0x3D,0xC6,0x0F,0x83,0x84,0x72,0x47,
    0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4
};

typedef struct __attribute__((packed)){
    uint64_t sig,rev_hdrsz; /* not real layout, use below */
} _pad_t;

typedef struct __attribute__((packed)){
    uint64_t sig;
    uint32_t rev,hdrsz,crc,res;
    uint64_t my_lba,alt_lba,first_use,last_use;
    uint8_t  disk_guid[16];
    uint64_t part_lba;
    uint32_t nparts,entsz,parts_crc;
} gpt_hdr_t;

typedef struct __attribute__((packed)){
    uint8_t  type[16],guid[16];
    uint64_t start,end,flags;
    uint16_t name[36];
} gpt_ent_t;

/* CRC32 */
static uint32_t crc32_table[256];
static bool     crc32_ready=false;
static void crc32_init(void){
    if(crc32_ready) return;
    for(uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        crc32_table[i]=c;
    }
    crc32_ready=true;
}
static uint32_t crc32(const void *buf,size_t len){
    crc32_init();
    uint32_t c=0xFFFFFFFFu;
    const uint8_t *p=(const uint8_t*)buf;
    while(len--) c=crc32_table[(c^*p++)&0xFF]^(c>>8);
    return c^0xFFFFFFFFu;
}

/* Simple GUID generator from seed */
static void make_guid(uint8_t *g,uint32_t seed){
    for(int i=0;i<16;i++){seed=seed*1664525+1013904223;g[i]=(uint8_t)(seed>>16);}
    g[6]=(g[6]&0x0F)|0x40; g[8]=(g[8]&0x3F)|0x80;
}

typedef struct{ uint64_t efi_s,efi_e,boot_s,boot_e,root_s,root_e; } layout_t;

static bool write_gpt(uint64_t total,layout_t *L){
    static uint8_t pt[128*128];
    k_memset(pt,0,sizeof pt);

    gpt_ent_t *ee=(gpt_ent_t*)(pt+0*128);
    k_memcpy(ee->type,GPT_EFI,16); make_guid(ee->guid,0x11111111);
    ee->start=L->efi_s; ee->end=L->efi_e;
    ee->name[0]='E';ee->name[1]='F';ee->name[2]='I';

    gpt_ent_t *be=(gpt_ent_t*)(pt+1*128);
    k_memcpy(be->type,GPT_LINUX,16); make_guid(be->guid,0x22222222);
    be->start=L->boot_s; be->end=L->boot_e;
    be->name[0]='b';be->name[1]='o';be->name[2]='o';be->name[3]='t';

    gpt_ent_t *re=(gpt_ent_t*)(pt+2*128);
    k_memcpy(re->type,GPT_LINUX,16); make_guid(re->guid,0x33333333);
    re->start=L->root_s; re->end=L->root_e;
    re->name[0]='/';

    uint32_t pcrc=crc32(pt,sizeof pt);

    static gpt_hdr_t hdr;
    k_memset(&hdr,0,sizeof hdr);
    hdr.sig=GPT_SIG; hdr.rev=0x00010000; hdr.hdrsz=92;
    hdr.my_lba=1; hdr.alt_lba=total-1;
    hdr.first_use=34; hdr.last_use=total-34;
    hdr.part_lba=2; hdr.nparts=128; hdr.entsz=128;
    hdr.parts_crc=pcrc;
    make_guid(hdr.disk_guid,0xDEADBEEF);
    hdr.crc=crc32(&hdr,92);

    /* Protective MBR */
    static uint8_t mbr[512];
    k_memset(mbr,0,512);
    mbr[446]=0x00; mbr[447]=0x00; mbr[448]=0x02; mbr[449]=0x00;
    mbr[450]=0xEE;
    mbr[451]=0xFF; mbr[452]=0xFF; mbr[453]=0xFF;
    mbr[454]=0x01; mbr[455]=0x00; mbr[456]=0x00; mbr[457]=0x00;
    uint32_t sz=(uint32_t)((total-1>0xFFFFFFFF)?0xFFFFFFFF:(total-1));
    k_memcpy(&mbr[458],&sz,4);
    mbr[510]=0x55; mbr[511]=0xAA;

    if(!disk_write(0,1,mbr)) return false;

    /* GPT header at LBA 1 */
    static uint8_t hdr_sec[512];
    k_memset(hdr_sec,0,512);
    k_memcpy(hdr_sec,&hdr,sizeof hdr);
    if(!disk_write(1,1,hdr_sec)) return false;

    /* Partition table at LBA 2..33 (32 sectors = 16384 bytes) */
    uint8_t *pp=pt;
    for(int i=0;i<32;i++){
        if(!disk_write(2+i,1,pp)) return false;
        pp+=512;
    }
    return true;
}

/* ── ext4 quick format ───────────────────────────────────────────── */
static bool ext4_format_partition(uint64_t start_lba,uint64_t num_sectors){
    const uint32_t BS=4096, SPB=BS/512;
    uint64_t num_blocks=num_sectors/SPB;
    if(num_blocks<64) return false;

    uint32_t bpg=(uint32_t)(num_blocks<32768?num_blocks:32768);
    uint32_t num_inodes=bpg/4;
    uint32_t inode_size=256;
    uint32_t ipb=BS/inode_size;
    uint32_t itbl_blks=(num_inodes+ipb-1)/ipb;

    static uint8_t blk[4096];

    /* Zero first 16 blocks */
    k_memset(blk,0,BS);
    for(int i=0;i<16;i++){
        uint64_t lba=start_lba+(uint64_t)i*SPB;
        for(uint32_t s=0;s<SPB;s++)
            if(!disk_write(lba+s,1,blk+(s*512))) return false;
    }

    /* Superblock at partition byte 1024 = LBA+2, sector-offset 0 */
    static ext4_superblock_t sb;
    k_memset(&sb,0,sizeof sb);
    sb.s_inodes_count        = num_inodes;
    sb.s_blocks_count_lo     = (uint32_t)num_blocks;
    sb.s_free_blocks_count_lo= (uint32_t)num_blocks-4-itbl_blks-1;
    sb.s_free_inodes_count   = num_inodes-11;
    sb.s_first_data_block    = 0;
    sb.s_log_block_size      = 2;  /* 4096 */
    sb.s_log_frag_size       = 2;
    sb.s_blocks_per_group    = bpg;
    sb.s_frags_per_group     = bpg;
    sb.s_inodes_per_group    = num_inodes;
    sb.s_magic               = 0xEF53;
    sb.s_state               = 1;
    sb.s_rev_level           = 1;
    sb.s_first_ino           = 11;
    sb.s_inode_size          = (uint16_t)inode_size;
    sb.s_feature_incompat    = 0x2;

    /* Write SB at LBA start+2 (offset 1024 into partition) */
    k_memset(blk,0,BS);
    k_memcpy(blk,&sb,sizeof sb);
    if(!disk_write(start_lba+2,1,blk)) return false;
    if(!disk_write(start_lba+3,1,blk)) return false;

    /* Group descriptor at block 1 */
    static ext4_group_desc_t gd;
    k_memset(&gd,0,sizeof gd);
    gd.bg_block_bitmap_lo=2;
    gd.bg_inode_bitmap_lo=3;
    gd.bg_inode_table_lo =4;
    gd.bg_free_blocks_count_lo=(uint16_t)(sb.s_free_blocks_count_lo&0xFFFF);
    gd.bg_free_inodes_count_lo=(uint16_t)(sb.s_free_inodes_count&0xFFFF);
    gd.bg_used_dirs_count_lo  =1;

    k_memset(blk,0,BS);
    k_memcpy(blk,&gd,sizeof gd);
    uint64_t gd_lba=start_lba+SPB;
    for(uint32_t s=0;s<SPB;s++)
        if(!disk_write(gd_lba+s,1,blk+s*512)) return false;

    /* Block bitmap */
    k_memset(blk,0,BS);
    uint32_t used=4+itbl_blks+1;
    for(uint32_t i=0;i<used;i++) blk[i/8]|=(1<<(i%8));
    uint64_t bb_lba=start_lba+2*SPB;
    for(uint32_t s=0;s<SPB;s++)
        if(!disk_write(bb_lba+s,1,blk+s*512)) return false;

    /* Inode bitmap */
    k_memset(blk,0,BS);
    for(int i=0;i<11;i++) blk[i/8]|=(1<<(i%8));
    uint64_t ib_lba=start_lba+3*SPB;
    for(uint32_t s=0;s<SPB;s++)
        if(!disk_write(ib_lba+s,1,blk+s*512)) return false;

    /* Inode table: set root inode (inode 2) */
    k_memset(blk,0,BS);
    ext4_inode_t *root=(ext4_inode_t*)(blk+inode_size); /* inode 2 = index 1 */
    root->i_mode      =EXT4_S_IFDIR|0755;
    root->i_links_count=2;
    root->i_size_lo   =BS;
    root->i_blocks_lo =SPB;
    uint64_t it_lba=start_lba+4*SPB;
    for(uint32_t s=0;s<SPB;s++)
        if(!disk_write(it_lba+s,1,blk+s*512)) return false;

    kprintf("    ext4: %llu blocks, %u inodes, block_size=%u\n",
            (unsigned long long)num_blocks, num_inodes, BS);
    return true;
}

/* ── GRUB config written to /boot partition ──────────────────────── */
static const char installed_grub_cfg[]=
    "set timeout=10\nset default=0\n"
    "insmod all_video\ninsmod gfxterm\ninsmod png\n"
    "loadfont /boot/grub/fonts/unicode.pf2\n"
    "terminal_output gfxterm\n"
    "set theme=/boot/grub/themes/bornomala/theme.txt\nexport theme\n"
    "menuentry \"  BornomalaOS\" --class bornomala {\n"
    "    multiboot2 /boot/mykernel.bin mode=live\n    boot\n}\n";

/* ── Write raw sectors ───────────────────────────────────────────── */
static bool write_raw(uint64_t lba,const void *data,size_t size){
    static uint8_t buf[512];
    const uint8_t *src=(const uint8_t*)data;
    while(size>0){
        size_t chunk=size>512?512:size;
        k_memset(buf,0,512);
        k_memcpy(buf,src,chunk);
        if(!disk_write(lba,1,buf)) return false;
        src+=chunk; size-=chunk; lba++;
    }
    return true;
}

/* ── Installer main ──────────────────────────────────────────────── */
void run_installer(void){
    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    terminal_writestring("\n  ╔══════════════════════════════════════╗\n");
    terminal_writestring("  ║     BornomalaOS Installer            ║\n");
    terminal_writestring("  ╚══════════════════════════════════════╝\n\n");

    /* Step 1: Disk detection */
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    kprintf("  [1/6] Detecting storage device...\n");

    if(!active_disk.present){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: No storage device found.\n");
        kprintf("  Make sure VirtualBox has a disk controller:\n");
        kprintf("    NVMe: Settings -> Storage -> Add NVMe Controller\n");
        kprintf("    SATA: Settings -> Storage -> SATA Controller -> Add disk\n\n");
        kprintf("  Press any key...\n");
        keyboard_getchar(); return;
    }

    terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
    kprintf("  Disk found:\n");
    kprintf("    Type   : %s\n",
            active_disk.type==DISK_TYPE_NVME?"NVMe":
            active_disk.type==DISK_TYPE_ATA ?"SATA/ATA/HDD":"Unknown");
    kprintf("    Model  : %s\n", active_disk.model);
    uint64_t gb=(active_disk.num_sectors*512)/(1024*1024*1024);
    kprintf("    Size   : ~%llu GB (%llu sectors)\n",
            (unsigned long long)gb,
            (unsigned long long)active_disk.num_sectors);

    /* Step 2: Confirm */
    terminal_setcolor(VGA_YELLOW,VGA_BLACK);
    kprintf("\n  WARNING: This will ERASE ALL DATA on the disk.\n");
    kprintf("  Type YES to continue: ");
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    char confirm[16]={0}; kscanf("%s",confirm);
    if(k_strcmp(confirm,"YES")!=0){
        kprintf("\n  Installation cancelled.\n");
        keyboard_getchar(); return;
    }

    /* Step 3: Partition layout */
    kprintf("\n  [2/6] Partitioning disk...\n");
    uint64_t total=active_disk.num_sectors;
    uint64_t spmb=(1024*1024)/512;  /* sectors per MB (always 512-byte) */

    layout_t L;
    L.efi_s =34;
    L.efi_e =L.efi_s +(100*spmb)-1;   /* 100 MB EFI  */
    L.boot_s=L.efi_e +1;
    L.boot_e=L.boot_s+(512*spmb)-1;   /* 512 MB /boot */
    L.root_s=L.boot_e+1;
    L.root_e=total-34-1;               /* rest = /    */

    if(L.root_e<=L.root_s||(L.root_e-L.root_s)<(512*spmb)){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: Disk too small (need at least 2 GB)\n");
        keyboard_getchar(); return;
    }

    if(!write_gpt(total,&L)){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: GPT write failed\n");
        keyboard_getchar(); return;
    }
    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    kprintf("    EFI  : LBA %llu - %llu\n",(unsigned long long)L.efi_s,(unsigned long long)L.efi_e);
    kprintf("    /boot: LBA %llu - %llu\n",(unsigned long long)L.boot_s,(unsigned long long)L.boot_e);
    kprintf("    /    : LBA %llu - %llu\n",(unsigned long long)L.root_s,(unsigned long long)L.root_e);

    /* Step 4: Format */
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    kprintf("\n  [3/6] Formatting /boot as ext4...\n");
    if(!ext4_format_partition(L.boot_s,L.boot_e-L.boot_s+1)){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: Failed to format /boot\n");
        keyboard_getchar(); return;
    }
    kprintf("  [4/6] Formatting / as ext4...\n");
    if(!ext4_format_partition(L.root_s,L.root_e-L.root_s+1)){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: Failed to format /\n");
        keyboard_getchar(); return;
    }

    /* Step 5: Write kernel binary */
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    kprintf("\n  [5/6] Installing kernel...\n");
    uint64_t kernel_lba=L.boot_s+1024;
    uint8_t *ksrc=(uint8_t*)0x100000;
    size_t  ksz=0x200000;  /* scan backwards for actual size */
    while(ksz>4096&&ksrc[ksz-1]==0) ksz-=4096;

    if(!write_raw(kernel_lba,ksrc,ksz)){
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        kprintf("  ERROR: Kernel write failed\n");
        keyboard_getchar(); return;
    }
    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    kprintf("    Kernel: %u KB at LBA %llu\n",
            (unsigned)(ksz/1024),(unsigned long long)kernel_lba);

    /* Write GRUB config */
    write_raw(L.boot_s+2048,(const uint8_t*)installed_grub_cfg,
              k_strlen(installed_grub_cfg));

    /* Step 6: EFI marker */
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    kprintf("\n  [6/6] Writing boot marker...\n");
    static uint8_t efi_mark[512];
    k_memset(efi_mark,0,512);
    const char *sig="BORNOMALA_V1";
    k_memcpy(efi_mark,sig,k_strlen(sig));
    k_memcpy(efi_mark+64,&kernel_lba,8);
    disk_write(L.efi_s,1,efi_mark);

    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    kprintf("\n  ╔══════════════════════════════════════╗\n");
    kprintf("  ║  Installation complete!              ║\n");
    kprintf("  ║  Remove the ISO and reboot.          ║\n");
    kprintf("  ╚══════════════════════════════════════╝\n\n");

    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    kprintf("  Press any key to reboot...\n");
    keyboard_getchar();

    /* Reboot via keyboard controller */
    uint8_t v=0x02;
    while(v&0x02) __asm__ volatile("inb $0x64,%0":"=a"(v));
    __asm__ volatile("outb %0,$0x64"::"a"((uint8_t)0xFE));
    for(;;) __asm__("hlt");
}