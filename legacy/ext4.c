/* kernel/ext4.c — ext4 driver (uses disk abstraction layer) */
#include "ext4.h"
#include "disk.h"
#include "alloc.h"
#include "kernel.h"

#define MAX_FILENAME 255

ext4_ctx_t ext4_context = {0};

/* ── GPT partition scanner ───────────────────────────────────────── */
/* Finds the first Linux-type partition and returns its start LBA.
   Falls back to LBA 2048 if no GPT found (raw / manually formatted). */

#define GPT_SIG 0x5452415020494645ULL

typedef struct __attribute__((packed)){
    uint64_t sig; uint32_t rev,hdrsz,crc,res;
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

static const uint8_t LINUX_TYPE[16]={
    0xAF,0x3D,0xC6,0x0F,0x83,0x84,0x72,0x47,
    0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4
};

/* Scan GPT, return start LBA of first Linux partition.
   Returns 0 if not found. */
static uint64_t gpt_find_root(void){
    static uint8_t sec[512];
    if(!disk_read(1,1,sec)) return 0;
    gpt_hdr_t *h=(gpt_hdr_t*)sec;
    if(h->sig!=GPT_SIG) return 0;

    /* Read partition entries — up to 128, 128 bytes each = 32 sectors */
    static uint8_t pt[128*128];
    uint32_t pt_sects=(h->nparts*h->entsz+511)/512;
    if(pt_sects>32) pt_sects=32;
    if(!disk_read(h->part_lba,pt_sects,pt)) return 0;

    /* Skip EFI (first) and /boot (second) — find first Linux /root */
    int linux_count=0;
    for(uint32_t i=0;i<h->nparts;i++){
        gpt_ent_t *e=(gpt_ent_t*)(pt+i*h->entsz);
        if(!e->start) continue;
        bool is_linux=true;
        for(int j=0;j<16;j++) if(e->type[j]!=LINUX_TYPE[j]){is_linux=false;break;}
        if(is_linux){
            linux_count++;
            if(linux_count==2) return e->start; /* second Linux = / */
            /* first Linux = /boot, skip */
        }
    }
    /* If only one Linux partition found, use it */
    if(linux_count==1){
        for(uint32_t i=0;i<h->nparts;i++){
            gpt_ent_t *e=(gpt_ent_t*)(pt+i*h->entsz);
            if(!e->start) continue;
            bool is_linux=true;
            for(int j=0;j<16;j++) if(e->type[j]!=LINUX_TYPE[j]){is_linux=false;break;}
            if(is_linux) return e->start;
        }
    }
    return 0;
}

/* ── Block I/O (uses disk layer) ─────────────────────────────────── */
static uint64_t partition_start_lba = 0;

bool ext4_read_block(uint32_t block_num, uint8_t *buf){
    uint32_t sects = ext4_context.block_size / 512;
    uint64_t lba   = partition_start_lba + (uint64_t)block_num * sects;
    return disk_read(lba, sects, buf);
}

bool ext4_write_block(uint32_t block_num, uint8_t *buf){
    uint32_t sects = ext4_context.block_size / 512;
    uint64_t lba   = partition_start_lba + (uint64_t)block_num * sects;
    return disk_write(lba, sects, buf);
}

/* ── Inode helpers ───────────────────────────────────────────────── */
static uint32_t grp_of(uint32_t i){ return (i-1)/ext4_context.sb.s_inodes_per_group; }
static uint32_t off_of(uint32_t i){ return ((i-1)%ext4_context.sb.s_inodes_per_group)*ext4_context.inode_size; }

bool ext4_read_inode(uint32_t ino,ext4_inode_t *out){
    if(ino<1||ino>ext4_context.sb.s_inodes_count) return false;
    uint32_t grp=grp_of(ino),off=off_of(ino);
    uint32_t tbl=ext4_context.group_descs[grp].bg_inode_table_lo;
    uint32_t blk=tbl+off/ext4_context.block_size;
    uint32_t boff=off%ext4_context.block_size;
    uint8_t *buf=kmalloc(ext4_context.block_size);
    if(!buf) return false;
    if(!ext4_read_block(blk,buf)){kfree(buf);return false;}
    *out=*(ext4_inode_t*)(buf+boff);
    kfree(buf); return true;
}

static bool ext4_write_inode(uint32_t ino,const ext4_inode_t *in){
    uint32_t grp=grp_of(ino),off=off_of(ino);
    uint32_t tbl=ext4_context.group_descs[grp].bg_inode_table_lo;
    uint32_t blk=tbl+off/ext4_context.block_size;
    uint32_t boff=off%ext4_context.block_size;
    uint8_t *buf=kmalloc(ext4_context.block_size);
    if(!buf) return false;
    if(!ext4_read_block(blk,buf)){kfree(buf);return false;}
    *(ext4_inode_t*)(buf+boff)=*in;
    bool ok=ext4_write_block(blk,buf);
    kfree(buf); return ok;
}

/* ── Block/inode allocation ──────────────────────────────────────── */
static uint32_t ext4_alloc_block(void){
    uint8_t *bm=kmalloc(ext4_context.block_size);
    if(!bm) return 0;
    for(uint32_t g=0;g<ext4_context.num_block_groups;g++){
        if(!ext4_read_block(ext4_context.group_descs[g].bg_block_bitmap_lo,bm)) continue;
        for(uint32_t i=0;i<ext4_context.sb.s_blocks_per_group;i++){
            if(!(bm[i/8]&(1<<(i%8)))){
                bm[i/8]|=(1<<(i%8));
                ext4_write_block(ext4_context.group_descs[g].bg_block_bitmap_lo,bm);
                ext4_context.group_descs[g].bg_free_blocks_count_lo--;
                ext4_context.sb.s_free_blocks_count_lo--;
                kfree(bm);
                return g*ext4_context.sb.s_blocks_per_group+i;
            }
        }
    }
    kfree(bm); return 0;
}

static uint32_t ext4_alloc_inode(void){
    uint8_t *bm=kmalloc(ext4_context.block_size);
    if(!bm) return 0;
    for(uint32_t g=0;g<ext4_context.num_block_groups;g++){
        if(!ext4_read_block(ext4_context.group_descs[g].bg_inode_bitmap_lo,bm)) continue;
        for(uint32_t i=0;i<ext4_context.sb.s_inodes_per_group;i++){
            if(!(bm[i/8]&(1<<(i%8)))){
                bm[i/8]|=(1<<(i%8));
                ext4_write_block(ext4_context.group_descs[g].bg_inode_bitmap_lo,bm);
                ext4_context.group_descs[g].bg_free_inodes_count_lo--;
                ext4_context.sb.s_free_inodes_count--;
                kfree(bm);
                return g*ext4_context.sb.s_inodes_per_group+i+1;
            }
        }
    }
    kfree(bm); return 0;
}

/* ── File block lookup ───────────────────────────────────────────── */
static uint32_t get_file_block(ext4_inode_t *ino,uint32_t logical){
    if(logical<12) return ino->i_block[logical];
    return 0;
}

/* ── Read/write file data ────────────────────────────────────────── */
bool ext4_read_file(uint32_t ino_num,uint64_t offset,size_t size,uint8_t *buf){
    ext4_inode_t ino;
    if(!ext4_read_inode(ino_num,&ino)) return false;
    uint64_t fsz=ino.i_size_lo|((uint64_t)ino.i_size_hi<<32);
    if(offset>=fsz) return false;
    if(offset+size>fsz) size=(size_t)(fsz-offset);
    uint8_t *tmp=kmalloc(ext4_context.block_size);
    if(!tmp) return false;
    uint32_t bi=(uint32_t)(offset/ext4_context.block_size);
    uint32_t bo=(uint32_t)(offset%ext4_context.block_size);
    size_t done=0;
    while(done<size){
        uint32_t phys=get_file_block(&ino,bi);
        if(!phys){kfree(tmp);return false;}
        if(!ext4_read_block(phys,tmp)){kfree(tmp);return false;}
        size_t av=ext4_context.block_size-bo;
        size_t cp=av<(size-done)?av:(size-done);
        k_memcpy(buf+done,tmp+bo,cp);
        done+=cp; bi++; bo=0;
    }
    kfree(tmp); return true;
}

bool ext4_write_file(uint32_t ino_num,uint64_t offset,size_t size,uint8_t *buf){
    ext4_inode_t ino;
    if(!ext4_read_inode(ino_num,&ino)) return false;
    uint8_t *tmp=kmalloc(ext4_context.block_size);
    if(!tmp) return false;
    uint32_t bi=(uint32_t)(offset/ext4_context.block_size);
    uint32_t bo=(uint32_t)(offset%ext4_context.block_size);
    size_t done=0;
    while(done<size){
        uint32_t phys=get_file_block(&ino,bi);
        if(!phys){
            phys=ext4_alloc_block();
            if(!phys){kfree(tmp);return false;}
            if(bi<12) ino.i_block[bi]=phys;
            k_memset(tmp,0,ext4_context.block_size);
        } else {
            if(!ext4_read_block(phys,tmp)){kfree(tmp);return false;}
        }
        size_t av=ext4_context.block_size-bo;
        size_t cp=av<(size-done)?av:(size-done);
        k_memcpy(tmp+bo,buf+done,cp);
        if(!ext4_write_block(phys,tmp)){kfree(tmp);return false;}
        done+=cp; bi++; bo=0;
    }
    uint64_t new_sz=offset+size;
    uint64_t old_sz=ino.i_size_lo|((uint64_t)ino.i_size_hi<<32);
    if(new_sz>old_sz){
        ino.i_size_lo=(uint32_t)(new_sz&0xFFFFFFFF);
        ino.i_size_hi=(uint32_t)(new_sz>>32);
    }
    ino.i_blocks_lo=bi*(ext4_context.block_size/512);
    ext4_write_inode(ino_num,&ino);
    kfree(tmp); return true;
}

/* ── Directory iteration ─────────────────────────────────────────── */
bool ext4_list_dir(uint32_t ino_num,ext4_dir_callback_t cb,void *ctx){
    ext4_inode_t ino;
    if(!ext4_read_inode(ino_num,&ino)) return false;
    if((ino.i_mode&EXT4_S_ITYPE)!=EXT4_S_IFDIR) return false;
    uint64_t dsz=ino.i_size_lo|((uint64_t)ino.i_size_hi<<32);
    uint8_t *dbuf=kmalloc((size_t)dsz);
    if(!dbuf) return false;
    if(!ext4_read_file(ino_num,0,(size_t)dsz,dbuf)){kfree(dbuf);return false;}
    uint64_t off=0;
    while(off<dsz){
        ext4_dir_entry_2_t *e=(ext4_dir_entry_2_t*)(dbuf+off);
        if(!e->rec_len) break;
        if(e->inode&&e->name_len){
            char name[MAX_FILENAME+1];
            k_memcpy(name,e->name,e->name_len); name[e->name_len]='\0';
            if(cb&&!cb(name,e->inode,e->file_type,ctx)) break;
        }
        off+=e->rec_len;
    }
    kfree(dbuf); return true;
}

/* ── Find inode by name ──────────────────────────────────────────── */
typedef struct{const char *name;uint32_t found;} fctx_t;
static bool fcb(const char *n,uint32_t ino,uint8_t ft,void *ctx){
    (void)ft; fctx_t *fc=(fctx_t*)ctx;
    if(k_strcmp(n,fc->name)==0){fc->found=ino;return false;}
    return true;
}
uint32_t ext4_find_inode_in_dir(uint32_t parent,const char *name){
    fctx_t fc={name,0}; ext4_list_dir(parent,fcb,&fc); return fc.found;
}
uint32_t ext4_find_inode(const char *path){
    if(!path||*path!='/') return 0;
    uint32_t cur=2; const char *p=path+1;
    while(*p){
        char seg[MAX_FILENAME+1]; int len=0;
        while(*p&&*p!='/') seg[len++]=*p++;
        seg[len]='\0';
        if(!len){if(*p)p++;continue;}
        cur=ext4_find_inode_in_dir(cur,seg);
        if(!cur) return 0;
        if(*p) p++;
    }
    return cur;
}

/* ── Directory entry management ──────────────────────────────────── */
bool ext4_add_dir_entry(uint32_t parent,const char *name,uint32_t ino_num,uint8_t ftype){
    ext4_inode_t pino;
    if(!ext4_read_inode(parent,&pino)) return false;
    uint64_t dsz=pino.i_size_lo|((uint64_t)pino.i_size_hi<<32);
    uint8_t *dbuf=kmalloc((size_t)dsz+ext4_context.block_size);
    if(!dbuf) return false;
    k_memset(dbuf,0,(size_t)dsz+ext4_context.block_size);
    if(dsz) ext4_read_file(parent,0,(size_t)dsz,dbuf);
    uint8_t nlen=(uint8_t)k_strlen(name);
    uint16_t need=(uint16_t)((sizeof(ext4_dir_entry_2_t)+nlen+3)&~3);
    uint64_t off=0; bool placed=false;
    while(off<dsz){
        ext4_dir_entry_2_t *e=(ext4_dir_entry_2_t*)(dbuf+off);
        if(!e->rec_len) break;
        uint16_t real=(uint16_t)((sizeof(ext4_dir_entry_2_t)+e->name_len+3)&~3);
        if(!e->inode&&e->rec_len>=need){
            e->inode=ino_num; e->name_len=nlen; e->file_type=ftype;
            k_memcpy(e->name,name,nlen); placed=true; break;
        }
        if(e->rec_len-real>=need){
            uint16_t old=e->rec_len; e->rec_len=real;
            ext4_dir_entry_2_t *ne=(ext4_dir_entry_2_t*)(dbuf+off+real);
            ne->inode=ino_num; ne->rec_len=old-real;
            ne->name_len=nlen; ne->file_type=ftype;
            k_memcpy(ne->name,name,nlen); placed=true; break;
        }
        off+=e->rec_len;
    }
    if(!placed){
        uint32_t phys=ext4_alloc_block();
        if(!phys){kfree(dbuf);return false;}
        if((dsz/ext4_context.block_size)<12) pino.i_block[dsz/ext4_context.block_size]=phys;
        ext4_dir_entry_2_t *ne=(ext4_dir_entry_2_t*)(dbuf+dsz);
        ne->inode=ino_num; ne->rec_len=(uint16_t)ext4_context.block_size;
        ne->name_len=nlen; ne->file_type=ftype;
        k_memcpy(ne->name,name,nlen);
        dsz+=ext4_context.block_size;
        pino.i_size_lo=(uint32_t)(dsz&0xFFFFFFFF);
        pino.i_size_hi=(uint32_t)(dsz>>32);
        ext4_write_inode(parent,&pino);
    }
    bool ok=ext4_write_file(parent,0,(size_t)dsz,dbuf);
    kfree(dbuf); return ok;
}

bool ext4_remove_dir_entry(uint32_t parent,const char *name){
    ext4_inode_t pino;
    if(!ext4_read_inode(parent,&pino)) return false;
    uint64_t dsz=pino.i_size_lo|((uint64_t)pino.i_size_hi<<32);
    uint8_t *dbuf=kmalloc((size_t)dsz);
    if(!dbuf) return false;
    if(!ext4_read_file(parent,0,(size_t)dsz,dbuf)){kfree(dbuf);return false;}
    uint64_t off=0;
    while(off<dsz){
        ext4_dir_entry_2_t *e=(ext4_dir_entry_2_t*)(dbuf+off);
        if(!e->rec_len) break;
        char n[MAX_FILENAME+1]; k_memcpy(n,e->name,e->name_len); n[e->name_len]='\0';
        if(e->inode&&k_strcmp(n,name)==0){
            e->inode=0;
            ext4_write_file(parent,0,(size_t)dsz,dbuf);
            kfree(dbuf); return true;
        }
        off+=e->rec_len;
    }
    kfree(dbuf); return false;
}

/* ── Create file / directory ─────────────────────────────────────── */
bool ext4_create_file(uint32_t parent,const char *name,uint16_t mode){
    uint32_t ino_num=ext4_alloc_inode();
    if(!ino_num) return false;
    ext4_inode_t ino; k_memset(&ino,0,sizeof ino);
    ino.i_mode=mode; ino.i_links_count=1;
    uint8_t ftype=((mode&EXT4_S_ITYPE)==EXT4_S_IFDIR)?EXT4_FT_DIR:EXT4_FT_REG_FILE;
    if(!ext4_write_inode(ino_num,&ino)) return false;
    return ext4_add_dir_entry(parent,name,ino_num,ftype);
}

bool ext4_create_dir(uint32_t parent,const char *name){
    uint32_t ino_num=ext4_alloc_inode();
    if(!ino_num) return false;
    ext4_inode_t ino; k_memset(&ino,0,sizeof ino);
    ino.i_mode=EXT4_S_IFDIR|0755; ino.i_links_count=2;
    uint32_t data_blk=ext4_alloc_block();
    if(!data_blk) return false;
    ino.i_block[0]=data_blk;
    ino.i_size_lo=ext4_context.block_size;
    ino.i_blocks_lo=ext4_context.block_size/512;
    uint8_t *buf=kmalloc(ext4_context.block_size);
    if(!buf) return false;
    k_memset(buf,0,ext4_context.block_size);
    ext4_dir_entry_2_t *dot=(ext4_dir_entry_2_t*)buf;
    dot->inode=ino_num; dot->rec_len=12; dot->name_len=1;
    dot->file_type=EXT4_FT_DIR; dot->name[0]='.';
    ext4_dir_entry_2_t *dd=(ext4_dir_entry_2_t*)(buf+12);
    dd->inode=parent; dd->rec_len=(uint16_t)(ext4_context.block_size-12);
    dd->name_len=2; dd->file_type=EXT4_FT_DIR;
    dd->name[0]='.'; dd->name[1]='.';
    ext4_write_block(data_blk,buf); kfree(buf);
    if(!ext4_write_inode(ino_num,&ino)) return false;
    return ext4_add_dir_entry(parent,name,ino_num,EXT4_FT_DIR);
}

bool ext4_delete_file(uint32_t parent,const char *name){
    uint32_t ino_num=ext4_find_inode_in_dir(parent,name);
    if(!ino_num) return false;
    ext4_inode_t ino;
    if(!ext4_read_inode(ino_num,&ino)) return false;
    ino.i_links_count=0; ino.i_dtime=1;
    ext4_write_inode(ino_num,&ino);
    return ext4_remove_dir_entry(parent,name);
}

bool ext4_write_path(const char *path,const uint8_t *data,size_t size){
    char pp[256]; int plen=0;
    const char *fname=path;
    for(const char *p=path;*p;p++) if(*p=='/') fname=p+1;
    plen=(int)(fname-path-1);
    if(plen<=0){pp[0]='/';pp[1]='\0';}
    else{k_memcpy(pp,path,plen);pp[plen]='\0';}
    uint32_t par=ext4_find_inode(plen>0?pp:"/");
    if(!par) return false;
    uint32_t fino=ext4_find_inode_in_dir(par,fname);
    if(!fino){
        if(!ext4_create_file(par,fname,EXT4_S_IFREG|0644)) return false;
        fino=ext4_find_inode_in_dir(par,fname);
        if(!fino) return false;
    }
    return ext4_write_file(fino,0,size,(uint8_t*)data);
}

/* ── Init: scan GPT for root partition, read superblock ──────────── */
bool ext4_init(void){
    if(!active_disk.present) return false;

    /* Try to find root partition via GPT scan */
    uint64_t root_start = gpt_find_root();

    /* If no GPT found, try LBA 2048 (common default for hand-formatted disks) */
    if(!root_start) root_start = 2048;

    /* Try superblock at byte offset 1024 into partition = LBA+2 */
    static uint8_t sb_buf[1024];
    if(!disk_read(root_start+2, 2, sb_buf)){
        /* try raw offset 0 (some tools write SB at start) */
        if(!disk_read(root_start, 2, sb_buf)) return false;
    }

    ext4_superblock_t *sb=(ext4_superblock_t*)sb_buf;
    if(sb->s_magic!=0xEF53) return false;

    partition_start_lba     = root_start;
    ext4_context.sb         = *sb;
    ext4_context.block_size = 1024<<sb->s_log_block_size;
    ext4_context.inode_size = sb->s_inode_size?sb->s_inode_size:128;

    if(ext4_context.block_size<1024||ext4_context.block_size>65536) return false;

    ext4_context.num_block_groups =
        (sb->s_blocks_count_lo+sb->s_blocks_per_group-1)/sb->s_blocks_per_group;

    uint32_t gd_bytes=ext4_context.num_block_groups*sizeof(ext4_group_desc_t);
    ext4_context.group_descs=(ext4_group_desc_t*)kmalloc(gd_bytes);
    if(!ext4_context.group_descs) return false;

    uint32_t gdt_blks=(gd_bytes+ext4_context.block_size-1)/ext4_context.block_size;
    uint8_t *gdt_buf=(uint8_t*)kmalloc(gdt_blks*ext4_context.block_size);
    if(!gdt_buf) return false;

    for(uint32_t i=0;i<gdt_blks;i++)
        if(!ext4_read_block(1+i,gdt_buf+i*ext4_context.block_size)){
            kfree(gdt_buf); return false;
        }
    k_memcpy(ext4_context.group_descs,gdt_buf,gd_bytes);
    kfree(gdt_buf);

    ext4_context.current_inode=2;
    return true;
}