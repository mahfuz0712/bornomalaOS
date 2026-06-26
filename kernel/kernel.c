/* kernel/kernel.c  —  64-bit bare-metal kernel
   VGA driver  +  PS/2 keyboard  +  kprintf / kscanf  +  ATA + ext4 */

#include "kernel.h"
#include "alloc.h"
#include "ext4.h"
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════
   PORT I/O
═══════════════════════════════════════════════════════════════════ */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

/* ═══════════════════════════════════════════════════════════════════
   VGA TEXT DRIVER
═══════════════════════════════════════════════════════════════════ */
static volatile uint16_t *const vga = (volatile uint16_t *)VGA_MEMORY;
static size_t   t_row, t_col;
static uint8_t  t_color;

static uint8_t  mk_color(vga_color_t fg, vga_color_t bg){ return fg|(bg<<4); }
static uint16_t mk_entry(char c, uint8_t col){ return (uint8_t)c|(uint16_t)col<<8; }

void terminal_setcolor(vga_color_t fg, vga_color_t bg){
    t_color = mk_color(fg, bg);
}

void terminal_init(void){
    t_row = t_col = 0;
    t_color = mk_color(VGA_LIGHT_GREY, VGA_BLACK);
    for(size_t y=0;y<VGA_HEIGHT;y++)
        for(size_t x=0;x<VGA_WIDTH;x++)
            vga[y*VGA_WIDTH+x] = mk_entry(' ', t_color);
}

static void terminal_scroll(void){
    for(size_t y=1;y<VGA_HEIGHT;y++)
        for(size_t x=0;x<VGA_WIDTH;x++)
            vga[(y-1)*VGA_WIDTH+x] = vga[y*VGA_WIDTH+x];
    for(size_t x=0;x<VGA_WIDTH;x++)
        vga[(VGA_HEIGHT-1)*VGA_WIDTH+x] = mk_entry(' ', t_color);
}

/* move hardware cursor */
static void update_cursor(void){
    uint16_t pos = t_row * VGA_WIDTH + t_col;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

void terminal_putchar(char c){
    if(c == '\r'){ t_col=0; update_cursor(); return; }
    if(c == '\b'){
        if(t_col>0) --t_col;
        vga[t_row*VGA_WIDTH+t_col] = mk_entry(' ', t_color);
        update_cursor(); return;
    }
    if(c == '\n'){
        t_col=0;
        if(++t_row == VGA_HEIGHT){ terminal_scroll(); t_row=VGA_HEIGHT-1; }
        update_cursor(); return;
    }
    vga[t_row*VGA_WIDTH+t_col] = mk_entry(c, t_color);
    if(++t_col == VGA_WIDTH){
        t_col=0;
        if(++t_row == VGA_HEIGHT){ terminal_scroll(); t_row=VGA_HEIGHT-1; }
    }
    update_cursor();
}

void terminal_writestring(const char *s){
    for(; *s; s++) terminal_putchar(*s);
}

void terminal_writehex(uint64_t n){
    const char *hex = "0123456789ABCDEF";
    terminal_writestring("0x");
    bool leading = true;
    for(int i=60;i>=0;i-=4){
        uint8_t d = (n>>i)&0xF;
        if(d||!leading||i==0){ terminal_putchar(hex[d]); leading=false; }
    }
}

void terminal_writedec(int64_t n){
    if(n<0){ terminal_putchar('-'); n=-n; }
    char buf[21]; int i=0;
    if(n==0){ terminal_putchar('0'); return; }
    while(n){ buf[i++]='0'+(n%10); n/=10; }
    while(i--) terminal_putchar(buf[i]);
}

/* ═══════════════════════════════════════════════════════════════════
   PS/2 KEYBOARD DRIVER
   Scancode set 1  (what VirtualBox sends by default)
═══════════════════════════════════════════════════════════════════ */
#define KBD_DATA    0x60
#define KBD_STATUS  0x64

/* US QWERTY scancode → ASCII (index = scancode, 0 = unmapped) */
static const char sc_map_lower[128] = {
/*00*/  0,   0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
/*10*/ 'q','w','e','r','t','y','u','i','o','p','[',']','\n',  0, 'a', 's',
/*20*/ 'd','f','g','h','j','k','l',';','\'','`',  0,'\\','z','x','c','v',
/*30*/ 'b','n','m',',','.','/',  0, '*',  0, ' ',  0,   0,   0,  0,  0,  0,
/*40*/   0,  0,  0,  0,  0,  0,  0, '7','8','9','-','4','5','6','+','1',
/*50*/ '2','3','0','.',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const char sc_map_upper[128] = {
/*00*/  0,   0, '!','@','#','$','%','^','&','*','(',')','_','+', '\b', '\t',
/*10*/ 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',  0, 'A', 'S',
/*20*/ 'D','F','G','H','J','K','L',':','"', '~',  0, '|', 'Z','X','C','V',
/*30*/ 'B','N','M','<','>','?',  0, '*',  0, ' ',  0,  0,  0,  0,  0,  0,
};

static bool shift_held  = false;
static bool caps_lock   = false;

static uint8_t kbd_read_scancode(void){
    while(!(inb(KBD_STATUS) & 0x01));   /* wait for data ready */
    return inb(KBD_DATA);
}

void keyboard_init(void){
    /* flush any pending bytes */
    while(inb(KBD_STATUS) & 0x01) inb(KBD_DATA);
}

char keyboard_getchar(void){
    while(1){
        uint8_t sc = kbd_read_scancode();
        bool released = (sc & 0x80) != 0;
        sc &= 0x7F;

        /* track shift keys (scancodes 0x2A = left, 0x36 = right) */
        if(sc == 0x2A || sc == 0x36){ shift_held = !released; continue; }
        /* caps lock toggle on press only */
        if(sc == 0x3A && !released){ caps_lock = !caps_lock; continue; }
        /* ignore all other key-release events */
        if(released) continue;
        /* ignore unmapped scancodes */
        if(sc >= 128) continue;

        bool upper = shift_held ^ caps_lock;
        char c = upper ? sc_map_upper[sc] : sc_map_lower[sc];
        if(c) return c;
    }
}

/* ═══════════════════════════════════════════════════════════════════
   kgets  —  read one line with echo + backspace support
═══════════════════════════════════════════════════════════════════ */
char *kgets(char *buf, int max){
    int i = 0;
    while(i < max-1){
        char c = keyboard_getchar();
        if(c == '\n' || c == '\r'){
            terminal_putchar('\n');
            break;
        }
        if(c == '\b'){
            if(i>0){ --i; terminal_putchar('\b'); }
            continue;
        }
        buf[i++] = c;
        terminal_putchar(c);
    }
    buf[i] = '\0';
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════
   UTILITY: string helpers (no libc available)
═══════════════════════════════════════════════════════════════════ */
static int k_strlen(const char *s){ int n=0; while(*s++) n++; return n; }

static int64_t k_atoi(const char *s){
    int64_t v=0; bool neg=false;
    if(*s=='-'){ neg=true; s++; }
    while(*s>='0'&&*s<='9') v=v*10+(*s++-'0');
    return neg?-v:v;
}

static uint64_t k_atou_hex(const char *s){
    uint64_t v=0;
    if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')) s+=2;
    while(1){
        char c=*s++;
        if(c>='0'&&c<='9') v=v*16+(c-'0');
        else if(c>='a'&&c<='f') v=v*16+(c-'a'+10);
        else if(c>='A'&&c<='F') v=v*16+(c-'A'+10);
        else break;
    }
    return v;
}

/* ═══════════════════════════════════════════════════════════════════
   kprintf  —  minimal printf: %c %s %d %u %x %lld %llu
═══════════════════════════════════════════════════════════════════ */
void kprintf(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    for(; *fmt; fmt++){
        if(*fmt != '%'){ terminal_putchar(*fmt); continue; }
        fmt++;
        bool lng = false;
        if(*fmt == 'l'){ lng = true; fmt++; }
        if(*fmt == 'l')  fmt++;          /* skip second l in %lld */
        switch(*fmt){
        case 'c': terminal_putchar((char)va_arg(ap, int)); break;
        case 's': terminal_writestring(va_arg(ap, const char*)); break;
        case 'd': terminal_writedec(lng ? va_arg(ap,int64_t) : va_arg(ap,int)); break;
        case 'u': terminal_writedec((int64_t)(lng ? va_arg(ap,uint64_t) : (uint64_t)va_arg(ap,unsigned))); break;
        case 'x': terminal_writehex(lng ? va_arg(ap,uint64_t) : va_arg(ap,unsigned)); break;
        case '%': terminal_putchar('%'); break;
        default:  terminal_putchar('?'); break;
        }
    }
    va_end(ap);
}

/* ═══════════════════════════════════════════════════════════════════
   kscanf  —  scanf-like:  %s  %d  %u  %c  %x
   Reads one full line, then parses it.
═══════════════════════════════════════════════════════════════════ */
int kscanf(const char *fmt, ...){
    char line[256];
    kgets(line, sizeof(line));

    va_list ap;
    va_start(ap, fmt);
    int matched = 0;
    const char *p = line;

    for(; *fmt; fmt++){
        if(*fmt != '%'){ if(*p==*fmt) p++; continue; }
        fmt++;

        /* skip whitespace in input */
        while(*p==' '||*p=='\t') p++;

        switch(*fmt){
        case 'd': {
            int64_t v = k_atoi(p);
            *va_arg(ap, int*) = (int)v;
            while(*p=='-'||(*p>='0'&&*p<='9')) p++;
            matched++;
            break;
        }
        case 'u': {
            int64_t v = k_atoi(p);
            *va_arg(ap, unsigned*) = (unsigned)v;
            while(*p>='0'&&*p<='9') p++;
            matched++;
            break;
        }
        case 'x': {
            uint64_t v = k_atou_hex(p);
            *va_arg(ap, unsigned*) = (unsigned)v;
            while(*p=='0'||*p=='x'||(*p>='0'&&*p<='9')||
                  (*p>='a'&&*p<='f')||(*p>='A'&&*p<='F')) p++;
            matched++;
            break;
        }
        case 's': {
            char *out = va_arg(ap, char*);
            while(*p && *p!=' ' && *p!='\t') *out++ = *p++;
            *out = '\0';
            matched++;
            break;
        }
        case 'c': {
            *va_arg(ap, char*) = *p ? *p++ : '\0';
            matched++;
            break;
        }
        default: break;
        }
    }
    va_end(ap);
    return matched;
}

/* ═══════════════════════════════════════════════════════════════════
   FILE SYSTEM COMMAND HELPERS
═══════════════════════════════════════════════════════════════════ */

static bool fs_list_callback(const char *name, uint32_t inode_num, uint8_t file_type, void *ctx) {
    (void)inode_num;
    (void)ctx;
    const char *type_str = "?";
    if (file_type == EXT4_FT_REG_FILE) type_str = "file";
    else if (file_type == EXT4_FT_DIR) type_str = "dir";
    else if (file_type == EXT4_FT_SYMLINK) type_str = "link";

    kprintf("    %s (%s)\n", name, type_str);
    return true;
}

static void edit_file(const char *filename) {
    kprintf("  Editing: %s (empty line to save)\n", filename);

    char line[256];
    size_t total_size = 0;
    uint8_t *buffer = (uint8_t *)kmalloc(65536); /* 64KB max file */

    if (!buffer) {
        kprintf("  Error: not enough memory\n");
        return;
    }

    terminal_setcolor(VGA_YELLOW, VGA_BLACK);

    while (total_size < 65536) {
        kprintf("  > ");
        kgets(line, sizeof(line));

        if (line[0] == '\0') {
            /* Empty line - save file */
            break;
        }

        /* Append line to buffer */
        int len = k_strlen(line);
        if (total_size + len + 1 > 65536) {
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            kprintf("  File too large\n");
            terminal_setcolor(VGA_YELLOW, VGA_BLACK);
            break;
        }

        for (int i = 0; i < len; i++) {
            buffer[total_size++] = line[i];
        }
        buffer[total_size++] = '\n';
    }

    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    kprintf("  Saved %u bytes to %s\n", (unsigned)total_size, filename);
    kfree(buffer);
}

/* ═══════════════════════════════════════════════════════════════════
   KERNEL MAIN
═══════════════════════════════════════════════════════════════════ */

static uint32_t fs_current_dir = 2; /* root inode */

void kernel_main(void *mboot_info){
    (void)mboot_info;

    terminal_init();
    keyboard_init();
    /* 4 MB heap at 2 MB mark */
    alloc_init((void*)0x200000, 0x400000);

    /* Detect disk: NVMe first, then SATA/ATA/HDD */
    disk_init();

    // boot_mode_t mode = multiboot_parse_mode(mboot_info);

    // if(mode == BOOT_MODE_INSTALL)
    //     run_installer();
    // else
    //     run_live();
    /* Initialize memory allocator (use 1MB heap at 0x200000) */
    alloc_init((void *)0x200000, 0x100000);

    /* Initialize disk and file system */
    ata_init();
    if (!ext4_init()) {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_writestring("  ERROR: ext4 initialization failed\n");
        for(;;) __asm__("hlt");
    }

    /* ── Banner ── */
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writestring("  ╔═════════════════════╗\n");
    terminal_writestring("  ║   bornomalaOS       ║\n");
    terminal_writestring("  ╚═════════════════════╝\n\n");

    /* ── Demo: read a name ── */
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    kprintf("  Enter your name: ");

    terminal_setcolor(VGA_YELLOW, VGA_BLACK);
    char name[64] = {0};
    kscanf("%s", name);

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("  Hello, %s!\n\n", name);

    /* ── Demo: read two numbers and add them ── */
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    kprintf("  Enter two integers (space-separated): ");

    terminal_setcolor(VGA_YELLOW, VGA_BLACK);
    int a = 0, b = 0;
    kscanf("%d %d", &a, &b);

    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("  %d + %d = %d\n\n", a, b, a+b);

    /* ── Interactive loop ── */
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    kprintf("  Type commands  (exit to quit):\n");
    kprintf("  > hello       — greet\n");
    kprintf("  > hex N       — print N in hex\n");
    kprintf("  > list        — list directory\n");
    kprintf("  > cd <path>   — change directory\n");
    kprintf("  > crtFile <f> — create file\n");
    kprintf("  > edit <f>    — edit file\n");
    kprintf("  > crtFolder <f> — create folder\n");
    kprintf("  > delFile <f> — delete file\n");
    kprintf("  > delFolder<f>— delete folder\n");
    kprintf("  > exit        — halt\n\n");

    while(1){
        terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
        kprintf("  kernel> ");
        terminal_setcolor(VGA_YELLOW, VGA_BLACK);

        char cmd[256] = {0};
        kscanf("%s", cmd);

        /* compare helper */
        #define STREQ(a,b) (k_strlen(a)==k_strlen(b) && \
            ({int _i=0,_r=1;while(a[_i]){if(a[_i]!=b[_i]){_r=0;break;}_i++;}(_r);}))

        terminal_setcolor(VGA_WHITE, VGA_BLACK);

        if(STREQ(cmd, "hello")){
            kprintf("  Hey there! Kernel says hi.\n");
        } else if(STREQ(cmd, "hex")){
            kprintf("  Enter a decimal number: ");
            terminal_setcolor(VGA_YELLOW, VGA_BLACK);
            int n = 0;
            kscanf("%d", &n);
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            kprintf("  Decimal %d = ", n);
            terminal_writehex((uint64_t)n);
            terminal_putchar('\n');
        } else if(STREQ(cmd, "list")){
            terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
            if (!ext4_list_dir(fs_current_dir, fs_list_callback, NULL)) {
                kprintf("  Error listing directory\n");
            }
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
        } else if(STREQ(cmd, "cd")){
            char path[256] = {0};
            kscanf("%s", path);
            uint32_t new_inode = ext4_find_inode(path);
            if (new_inode) {
                ext4_inode_t inode;
                if (ext4_read_inode(new_inode, &inode) && (inode.i_mode & EXT4_S_ITYPE) == EXT4_S_IFDIR) {
                    fs_current_dir = new_inode;
                    kprintf("  Directory changed.\n");
                } else {
                    kprintf("  Not a directory.\n");
                }
            } else {
                kprintf("  Path not found.\n");
            }
        } else if(STREQ(cmd, "crtFile")){
            char filename[256] = {0};
            kscanf("%s", filename);
            kprintf("  Creating file: %s\n", filename);
            kprintf("  (file creation not yet implemented)\n");
        } else if(STREQ(cmd, "edit")){
            char filename[256] = {0};
            kscanf("%s", filename);
            uint32_t inode_num = ext4_find_inode_in_dir(fs_current_dir, filename);
            if (inode_num) {
                terminal_setcolor(VGA_YELLOW, VGA_BLACK);
                edit_file(filename);
                terminal_setcolor(VGA_WHITE, VGA_BLACK);
            } else {
                terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
                edit_file(filename);
                terminal_setcolor(VGA_WHITE, VGA_BLACK);
            }
        } else if(STREQ(cmd, "crtFolder")){
            char foldername[256] = {0};
            kscanf("%s", foldername);
            kprintf("  Creating folder: %s\n", foldername);
            kprintf("  (folder creation not yet implemented)\n");
        } else if(STREQ(cmd, "delFile")){
            char filename[256] = {0};
            kscanf("%s", filename);
            kprintf("  Delete %s? (Y/n) ", filename);
            char response = keyboard_getchar();
            if (response == 'Y' || response == 'y' || response == '\r' || response == '\n') {
                kprintf("Y\n  Deleted.\n");
            } else {
                kprintf("n\n  Cancelled.\n");
            }
        } else if(STREQ(cmd, "delFolder")){
            char foldername[256] = {0};
            kscanf("%s", foldername);
            kprintf("  Delete %s? (Y/n) ", foldername);
            char response = keyboard_getchar();
            if (response == 'Y' || response == 'y' || response == '\r' || response == '\n') {
                kprintf("Y\n  Deleted.\n");
            } else {
                kprintf("n\n  Cancelled.\n");
            }
        } else if(STREQ(cmd, "exit")){
            terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
            kprintf("\n  Halting. Goodbye!\n");
            break;
        } else {
            kprintf("  Unknown command: %s\n", cmd);
        }
    }

    for(;;) __asm__("hlt");
}