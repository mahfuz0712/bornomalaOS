/* kernel/disk.c — disk abstraction layer
   Probes NVMe first, then ATA primary master (SATA/HDD/SSD).
   All higher-level code (ext4, installer) calls disk_read/disk_write
   and never touches NVMe or ATA directly.                           */

#include "disk.h"
#include "kernel.h"

disk_t active_disk = {0};

/* ── port I/O helpers ─────────────────────────────────────────────── */
static inline uint8_t  _inb(uint16_t p){ uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t _inw(uint16_t p){ uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint32_t _inl(uint16_t p){ uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void _outb(uint16_t p,uint8_t  v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void _outl(uint16_t p,uint32_t v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }

/* ════════════════════════════════════════════════════════════════════
   NVMe (MMIO via PCI BAR0)
════════════════════════════════════════════════════════════════════ */
#define NVME_CC   0x14
#define NVME_CSTS 0x1C
#define NVME_AQA  0x24
#define NVME_ASQ  0x28
#define NVME_ACQ  0x30
#define NVME_CSTS_RDY  (1u<<0)
#define NVME_CSTS_CFS  (1u<<1)
#define NVME_CC_EN     (1u<<0)

typedef struct __attribute__((packed)){
    uint8_t opc,flags; uint16_t cid; uint32_t nsid;
    uint64_t reserved,mptr,prp1,prp2;
    uint32_t cdw10,cdw11,cdw12,cdw13,cdw14,cdw15;
} nvme_cmd_t;

typedef struct __attribute__((packed)){
    uint32_t dw0,dw1; uint16_t sqhd,sqid,cid; uint16_t status;
} nvme_cqe_t;

typedef struct __attribute__((packed)){
    uint64_t nsze,ncap,nuse;
    uint8_t nsfeat,nlbaf,flbas,pad[101];
    struct{ uint16_t ms; uint8_t ds,rp; } lbaf[16];
} nvme_id_ns_t;

#define AQ 4
#define IQ 16

static nvme_cmd_t asq[AQ] __attribute__((aligned(4096)));
static nvme_cqe_t acq[AQ] __attribute__((aligned(4096)));
static nvme_cmd_t isq[IQ] __attribute__((aligned(4096)));
static nvme_cqe_t icq[IQ] __attribute__((aligned(4096)));
static uint8_t    id_buf[4096] __attribute__((aligned(4096)));

static uint64_t nvme_base = 0;
static uint32_t nvme_nsid = 0;
static uint32_t nvme_bs   = 512;
static uint32_t dstrd     = 0;

static uint16_t as_tail=0, ac_head=0, ac_phase=1;
static uint16_t is_tail=0, ic_head=0, ic_phase=1;
static uint16_t io_cid=0;

static inline uint32_t nvrd32(uint32_t o){ return *(volatile uint32_t*)(nvme_base+o); }
static inline void     nvwr32(uint32_t o,uint32_t v){ *(volatile uint32_t*)(nvme_base+o)=v; }
static inline void     nvwr64(uint32_t o,uint64_t v){ *(volatile uint64_t*)(nvme_base+o)=v; }
static inline uint32_t sq_db(uint16_t q){ return 0x1000+q*8*(1<<dstrd); }
static inline uint32_t cq_db(uint16_t q){ return 0x1000+q*8*(1<<dstrd)+4; }

static void nvme_delay(void){ for(volatile int i=0;i<50000;i++); }

static bool nvme_admin(nvme_cmd_t *cmd){
    asq[as_tail]=*cmd; asq[as_tail].cid=as_tail;
    as_tail=(as_tail+1)%AQ;
    nvwr32(sq_db(0),as_tail);
    for(uint32_t t=200000;t--;){ nvme_delay();
        if((acq[ac_head].status&1)==ac_phase){
            bool ok=((acq[ac_head].status>>1)&0x7FFF)==0;
            ac_head=(ac_head+1)%AQ;
            if(!ac_head) ac_phase^=1;
            nvwr32(cq_db(0),ac_head);
            return ok;
        }
    }
    return false;
}

static bool nvme_io(uint8_t opc,uint64_t lba,uint32_t cnt,void *buf){
    nvme_cmd_t cmd={0};
    cmd.opc=opc; cmd.nsid=nvme_nsid; cmd.cid=++io_cid;
    cmd.prp1=(uint64_t)(uintptr_t)buf;
    cmd.cdw10=(uint32_t)(lba&0xFFFFFFFF);
    cmd.cdw11=(uint32_t)(lba>>32);
    cmd.cdw12=cnt-1;
    isq[is_tail]=cmd;
    is_tail=(is_tail+1)%IQ;
    nvwr32(sq_db(1),is_tail);
    for(uint32_t t=500000;t--;){ nvme_delay();
        if((icq[ic_head].status&1)==ic_phase){
            bool ok=((icq[ic_head].status>>1)&0x7FFF)==0;
            ic_head=(ic_head+1)%IQ;
            if(!ic_head) ic_phase^=1;
            nvwr32(cq_db(1),ic_head);
            return ok;
        }
    }
    return false;
}

/* PCI config */
static uint32_t pcird(uint8_t bus,uint8_t slot,uint8_t fn,uint8_t off){
    _outl(0xCF8,(1u<<31)|((uint32_t)bus<<16)|((uint32_t)slot<<11)|((uint32_t)fn<<8)|(off&0xFC));
    return _inl(0xCFC);
}
static void pciwr(uint8_t bus,uint8_t slot,uint8_t fn,uint8_t off,uint32_t v){
    _outl(0xCF8,(1u<<31)|((uint32_t)bus<<16)|((uint32_t)slot<<11)|((uint32_t)fn<<8)|(off&0xFC));
    _outl(0xCFC,v);
}

static bool nvme_probe(void){
    /* Scan PCI for NVMe (class 0x01, sub 0x08, prog-if 0x02) */
    uint8_t bus=0,slot=0,fn=0; bool found=false;
    for(uint16_t b=0;b<256&&!found;b++)
        for(uint8_t s=0;s<32&&!found;s++)
            for(uint8_t f=0;f<8;f++){
                uint32_t id=pcird((uint8_t)b,s,f,0);
                if(id==0xFFFFFFFF) continue;
                uint32_t cc=pcird((uint8_t)b,s,f,8);
                if(((cc>>24)&0xFF)==0x01&&((cc>>16)&0xFF)==0x08&&((cc>>8)&0xFF)==0x02){
                    bus=(uint8_t)b;slot=s;fn=f;found=true;break;
                }
            }
    if(!found) return false;

    /* Enable bus-master + mem space */
    uint32_t cmd=pcird(bus,slot,fn,4); cmd|=0x06; pciwr(bus,slot,fn,4,cmd);

    uint32_t bar0=pcird(bus,slot,fn,0x10)&~0xFu;
    uint32_t bar1=pcird(bus,slot,fn,0x14);
    nvme_base=bar0|((uint64_t)bar1<<32);
    if(!nvme_base) return false;

    uint64_t cap=*(volatile uint64_t*)(nvme_base+0);
    dstrd=(cap>>32)&0xF;

    /* disable, wait */
    uint32_t cc=nvrd32(NVME_CC); cc&=~NVME_CC_EN; nvwr32(NVME_CC,cc);
    for(uint32_t t=200000;t--;){ if(!(nvrd32(NVME_CSTS)&NVME_CSTS_RDY)) break; nvme_delay(); }

    k_memset(asq,0,sizeof asq); k_memset(acq,0,sizeof acq);
    k_memset(isq,0,sizeof isq); k_memset(icq,0,sizeof icq);

    nvwr32(NVME_AQA,((AQ-1)<<16)|(AQ-1));
    nvwr64(NVME_ASQ,(uint64_t)(uintptr_t)asq);
    nvwr64(NVME_ACQ,(uint64_t)(uintptr_t)acq);

    cc=(0<<4)|(0<<7)|(0<<11)|(0<<14)|(6<<16)|(4<<20)|NVME_CC_EN;
    nvwr32(NVME_CC,cc);
    for(uint32_t t=200000;t--;){
        uint32_t s=nvrd32(NVME_CSTS);
        if(s&NVME_CSTS_CFS) return false;
        if(s&NVME_CSTS_RDY) break;
        nvme_delay();
    }

    /* Create I/O CQ (admin cmd 0x05) */
    nvme_cmd_t c={0}; c.opc=0x05; c.prp1=(uint64_t)(uintptr_t)icq;
    c.cdw10=((IQ-1)<<16)|1; c.cdw11=1;
    if(!nvme_admin(&c)) return false;

    /* Create I/O SQ (admin cmd 0x01) */
    k_memset(&c,0,sizeof c); c.opc=0x01; c.prp1=(uint64_t)(uintptr_t)isq;
    c.cdw10=((IQ-1)<<16)|1; c.cdw11=(1<<16)|1;
    if(!nvme_admin(&c)) return false;

    /* Identify namespace 1 */
    k_memset(&c,0,sizeof c); c.opc=0x06; c.nsid=1;
    c.prp1=(uint64_t)(uintptr_t)id_buf; c.cdw10=0;
    k_memset(id_buf,0,sizeof id_buf);
    if(!nvme_admin(&c)) return false;

    nvme_id_ns_t *ns=(nvme_id_ns_t*)id_buf;
    uint8_t fi=ns->flbas&0x0F;
    nvme_bs=1u<<ns->lbaf[fi].ds;
    nvme_nsid=1;

    active_disk.type       = DISK_TYPE_NVME;
    active_disk.sector_size= nvme_bs;
    active_disk.num_sectors= ns->nsze * (nvme_bs/512);
    active_disk.present    = true;
    k_strcpy(active_disk.model, "NVMe Disk");
    return true;
}

/* ════════════════════════════════════════════════════════════════════
   ATA PIO  (Primary channel, master drive — covers SATA/HDD/SSD)
════════════════════════════════════════════════════════════════════ */
#define ATA_DATA   0x1F0
#define ATA_ERR    0x1F1
#define ATA_CNT    0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DEV    0x1F6
#define ATA_CMD    0x1F7
#define ATA_CTRL   0x3F6

static bool ata_poll_bsy(void){
    for(uint32_t t=500000;t--;){ if(!(_inb(ATA_CMD)&0x80)) return true; }
    return false;
}
static bool ata_poll_drq(void){
    for(uint32_t t=500000;t--;){
        uint8_t s=_inb(ATA_CMD);
        if(s&0x01) return false; /* error */
        if(s&0x08) return true;  /* DRQ */
    }
    return false;
}

static bool ata_identify(void){
    /* soft reset */
    _outb(ATA_CTRL,0x04); for(volatile int i=0;i<1000;i++); _outb(ATA_CTRL,0x00);
    if(!ata_poll_bsy()) return false;

    _outb(ATA_DEV,0xA0);   /* select master, LBA mode */
    _outb(ATA_CNT,0); _outb(ATA_LBA0,0); _outb(ATA_LBA1,0); _outb(ATA_LBA2,0);
    _outb(ATA_CMD,0xEC);   /* IDENTIFY */

    uint8_t st=_inb(ATA_CMD);
    if(!st) return false;                          /* no drive */
    if(!ata_poll_bsy()) return false;
    /* ATAPI drives set LBA1/2 non-zero — skip them */
    if(_inb(ATA_LBA1)||_inb(ATA_LBA2)) return false;
    if(!ata_poll_drq()) return false;

    uint16_t id[256];
    for(int i=0;i<256;i++) id[i]=_inw(ATA_DATA);

    /* model string: words 27-46, byte-swapped */
    char model[41];
    for(int i=0;i<20;i++){
        model[i*2]  =(char)(id[27+i]>>8);
        model[i*2+1]=(char)(id[27+i]&0xFF);
    }
    model[40]='\0';
    /* trim trailing spaces */
    for(int i=39;i>=0&&model[i]==' ';i--) model[i]='\0';

    /* LBA48 sector count: words 100-103 */
    uint64_t sects=0;
    if(id[83]&(1<<10)){  /* LBA48 supported */
        sects = (uint64_t)id[100]
              | ((uint64_t)id[101]<<16)
              | ((uint64_t)id[102]<<32)
              | ((uint64_t)id[103]<<48);
    } else {
        sects = (uint32_t)id[60] | ((uint32_t)id[61]<<16);
    }
    if(!sects) return false;

    active_disk.type        = DISK_TYPE_ATA;
    active_disk.sector_size = 512;
    active_disk.num_sectors = sects;
    active_disk.present     = true;
    k_strcpy(active_disk.model, model);
    return true;
}

static bool ata_do_read(uint64_t lba,uint16_t cnt,void *buf){
    if(!ata_poll_bsy()) return false;
    _outb(ATA_DEV,   0x40|((lba>>24)&0x0F));
    _outb(ATA_CNT,   (uint8_t)(cnt&0xFF));
    _outb(ATA_LBA0,  (uint8_t)(lba&0xFF));
    _outb(ATA_LBA1,  (uint8_t)((lba>>8)&0xFF));
    _outb(ATA_LBA2,  (uint8_t)((lba>>16)&0xFF));
    _outb(ATA_CMD,   0x20);
    uint8_t *p=(uint8_t*)buf;
    for(uint16_t i=0;i<cnt;i++){
        if(!ata_poll_drq()) return false;
        for(int j=0;j<256;j++){
            uint16_t w=_inw(ATA_DATA);
            p[j*2]=w&0xFF; p[j*2+1]=(w>>8)&0xFF;
        }
        p+=512; ata_poll_bsy();
    }
    return true;
}

static bool ata_do_write(uint64_t lba,uint16_t cnt,const void *buf){
    if(!ata_poll_bsy()) return false;
    _outb(ATA_DEV,   0x40|((lba>>24)&0x0F));
    _outb(ATA_CNT,   (uint8_t)(cnt&0xFF));
    _outb(ATA_LBA0,  (uint8_t)(lba&0xFF));
    _outb(ATA_LBA1,  (uint8_t)((lba>>8)&0xFF));
    _outb(ATA_LBA2,  (uint8_t)((lba>>16)&0xFF));
    _outb(ATA_CMD,   0x30);
    const uint8_t *p=(const uint8_t*)buf;
    for(uint16_t i=0;i<cnt;i++){
        if(!ata_poll_drq()) return false;
        for(int j=0;j<256;j++){
            uint16_t w=p[j*2]|(uint16_t)(p[j*2+1]<<8);
            __asm__ volatile("outw %0,%1"::"a"(w),"Nd"((uint16_t)ATA_DATA));
        }
        p+=512; ata_poll_bsy();
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════
   PUBLIC API
════════════════════════════════════════════════════════════════════ */
bool disk_init(void){
    active_disk.present=false;
    /* Try NVMe first, then ATA */
    if(nvme_probe())  return true;
    if(ata_identify()) return true;
    return false;
}

/* Read count 512-byte sectors from LBA into buf */
bool disk_read(uint64_t lba,uint32_t count,void *buf){
    if(!active_disk.present||!count) return false;
    if(active_disk.type==DISK_TYPE_NVME){
        /* NVMe native: one call (PRP covers up to 4K; split larger reads) */
        static uint8_t tmp[4096] __attribute__((aligned(4096)));
        uint32_t sects_per_blk = nvme_bs/512;
        uint8_t *out=(uint8_t*)buf;
        uint64_t nlba=lba; uint32_t rem=count;
        while(rem){
            uint32_t batch = rem<sects_per_blk?rem:sects_per_blk;
            uint32_t nvme_cnt=(batch+sects_per_blk-1)/sects_per_blk;
            if(!nvme_io(0x02,nlba/sects_per_blk,nvme_cnt,tmp)) return false;
            uint32_t copy=batch*512;
            k_memcpy(out,tmp+(nlba%sects_per_blk)*512,copy);
            out+=copy; nlba+=batch; rem-=batch;
        }
        return true;
    }
    /* ATA: max 255 sectors per call */
    uint8_t *out=(uint8_t*)buf;
    uint64_t nlba=lba; uint32_t rem=count;
    while(rem){
        uint16_t batch=(rem>255)?255:(uint16_t)rem;
        if(!ata_do_read(nlba,batch,out)) return false;
        out+=batch*512; nlba+=batch; rem-=batch;
    }
    return true;
}

/* Write count 512-byte sectors from buf to LBA */
bool disk_write(uint64_t lba,uint32_t count,const void *buf){
    if(!active_disk.present||!count) return false;
    if(active_disk.type==DISK_TYPE_NVME){
        static uint8_t tmp[4096] __attribute__((aligned(4096)));
        uint32_t sects_per_blk=nvme_bs/512;
        const uint8_t *in=(const uint8_t*)buf;
        uint64_t nlba=lba; uint32_t rem=count;
        while(rem){
            uint32_t batch=rem<sects_per_blk?rem:sects_per_blk;
            uint32_t nvme_cnt=(batch+sects_per_blk-1)/sects_per_blk;
            /* read-modify-write if partial nvme block */
            if(batch<sects_per_blk){
                if(!nvme_io(0x02,nlba/sects_per_blk,nvme_cnt,tmp)) return false;
            } else {
                k_memset(tmp,0,nvme_bs);
            }
            k_memcpy(tmp+(nlba%sects_per_blk)*512,in,batch*512);
            if(!nvme_io(0x01,nlba/sects_per_blk,nvme_cnt,tmp)) return false;
            in+=batch*512; nlba+=batch; rem-=batch;
        }
        return true;
    }
    /* ATA */
    const uint8_t *in=(const uint8_t*)buf;
    uint64_t nlba=lba; uint32_t rem=count;
    while(rem){
        uint16_t batch=(rem>255)?255:(uint16_t)rem;
        if(!ata_do_write(nlba,batch,in)) return false;
        in+=batch*512; nlba+=batch; rem-=batch;
    }
    return true;
}