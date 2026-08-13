/* =============================================================================
 *  RELIS — kernel/fs/stackfs.c
 *
 *  KEY FIX: replaced per-call kmalloc(4096) with a single static block
 *  buffer. The original static buffers in readdir/finddir caused corruption
 *  because readdir and finddir shared the same static buffer. The kmalloc
 *  fix worked logically but exhausted the 4MB heap during format.
 *
 *  Solution: TWO static buffers — one for readdir, one for finddir.
 *  They are never called re-entrantly (interrupts are off during ATA I/O),
 *  so two separate static buffers is safe and uses zero heap.
 *
 *  The root cause of the readdir/finddir conflict:
 *  - bfs_readdir fills static buf_rd, returns pointer into it
 *  - caller immediately calls bfs_finddir which fills static buf_fd
 *  - buf_rd and buf_fd are DIFFERENT buffers now → no corruption
 *  ============================================================================= */
#include "stackfs.h"
#include "vfs.h"
#include "drivers/disk/ata.h"
#include "mm/heap.h"
#include "kprintf.h"
#include <stddef.h>
#include <stdint.h>

static stackfs_super_t sb;
static stackfs_inode_t inodes[STACKFS_MAX_INODES];
static int             mounted      = 0;
static int             sb_dirty     = 0;
static int             inodes_dirty = 0;
static int             bitmap_dirty = 0;

/* ── TWO separate block buffers — one per operation type ─────────────────── */
/* buf_rw is used by bfs_read, bfs_write, dir_add_entry, bfs_unlink         */
/* buf_dir is used by bfs_readdir                                            */
/* buf_find is used by bfs_finddir                                           */
/* They are never used simultaneously because interrupts are off during I/O  */
static uint8_t buf_rw  [STACKFS_BLOCK_SIZE];
static uint8_t buf_dir [STACKFS_BLOCK_SIZE];
static uint8_t buf_find[STACKFS_BLOCK_SIZE];

/* ── Sector I/O ──────────────────────────────────────────────────────────── */
static int read_sector (uint32_t lba,void       *b){return ata_read (lba,1,b);}
static int write_sector(uint32_t lba,const void *b){return ata_write(lba,1,b);}

/* ── Inode table ─────────────────────────────────────────────────────────── */
static void load_inodes(void){
    uint8_t *dst=(uint8_t*)inodes;
    uint8_t  tmp[512];
    uint32_t total=(uint32_t)(STACKFS_MAX_INODES*sizeof(stackfs_inode_t)),done=0;
    for(int s=0;s<STACKFS_INODE_SECS&&done<total;s++){
        read_sector((uint32_t)(STACKFS_INODE_START+s),tmp);
        uint32_t n=512;if(done+n>total)n=total-done;
        for(uint32_t b=0;b<n;b++)dst[done+b]=tmp[b];
        done+=n;
    }
}
static void save_inodes_disk(void){
    const uint8_t *src=(const uint8_t*)inodes;
    uint8_t tmp[512];
    uint32_t total=(uint32_t)(STACKFS_MAX_INODES*sizeof(stackfs_inode_t)),done=0;
    for(int s=0;s<STACKFS_INODE_SECS&&done<total;s++){
        uint32_t n=512;if(done+n>total)n=total-done;
        for(int i=0;i<512;i++)tmp[i]=0;
        for(uint32_t b=0;b<n;b++)tmp[b]=src[done+b];
        write_sector((uint32_t)(STACKFS_INODE_START+s),tmp);
        done+=n;
    }
    inodes_dirty=0;
}

/* ── Superblock ──────────────────────────────────────────────────────────── */
static void save_sb_disk(void){write_sector(STACKFS_SB_SECTOR,&sb);sb_dirty=0;}

/* ── Bitmap ──────────────────────────────────────────────────────────────── */
#define BITMAP_BYTES (STACKFS_BITMAP_SECS*512)
static uint8_t bitmap[BITMAP_BYTES];
static void load_bitmap(void){
    for(int s=0;s<STACKFS_BITMAP_SECS;s++)
        read_sector((uint32_t)(STACKFS_BITMAP_START+s),bitmap+s*512);
}
static void save_bitmap_disk(void){
    for(int s=0;s<STACKFS_BITMAP_SECS;s++)
        write_sector((uint32_t)(STACKFS_BITMAP_START+s),bitmap+s*512);
    bitmap_dirty=0;
}

/* ── Sync ────────────────────────────────────────────────────────────────── */
static void stackfs_sync(void){
    if(inodes_dirty)save_inodes_disk();
    if(bitmap_dirty) save_bitmap_disk();
    if(sb_dirty)     save_sb_disk();
}

/* ── Bitmap alloc/free ───────────────────────────────────────────────────── */
static int bitmap_alloc(void){
    for(int i=0;i<(int)sb.total_blocks;i++){
        if(!(bitmap[i/8]&(1<<(i%8)))){
            bitmap[i/8]|=(uint8_t)(1<<(i%8));
            sb.free_blocks--;bitmap_dirty=1;sb_dirty=1;return i;
        }
    }
    return -1;
}
static void bitmap_free_blk(int b){
    if(b>=0){
        bitmap[b/8]&=(uint8_t)~(1<<(b%8));
        sb.free_blocks++;bitmap_dirty=1;sb_dirty=1;
    }
}

/* ── Block I/O ───────────────────────────────────────────────────────────── */
static uint32_t blk_lba(int b){
    return(uint32_t)(STACKFS_DATA_START+b*(STACKFS_BLOCK_SIZE/STACKFS_SECTOR_SIZE));
}
static int read_block(int b,void *buf){
    uint32_t lba=blk_lba(b);uint8_t *p=(uint8_t*)buf;
    for(int s=0;s<STACKFS_BLOCK_SIZE/STACKFS_SECTOR_SIZE;s++){
        if(ata_read(lba+(uint32_t)s,1,p)<0)return -1;p+=512;
    }
    return 0;
}
static int write_block(int b,const void *buf){
    uint32_t lba=blk_lba(b);const uint8_t *p=(const uint8_t*)buf;
    for(int s=0;s<STACKFS_BLOCK_SIZE/STACKFS_SECTOR_SIZE;s++){
        if(ata_write(lba+(uint32_t)s,1,p)<0)return -1;p+=512;
    }
    return 0;
}

/* ── Inode alloc ─────────────────────────────────────────────────────────── */
static int alloc_inode(void){
    for(int i=1;i<STACKFS_MAX_INODES;i++){
        if(inodes[i].type==SFS_TYPE_FREE){
            sb.free_inodes--;sb_dirty=1;inodes_dirty=1;return i;
        }
    }
    return -1;
}

/* ── String helpers ──────────────────────────────────────────────────────── */
static void bscpy(char *d,const char *s,int max){
    int i=0;while(i<max-1&&s[i]){d[i]=s[i];i++;}d[i]='\0';
}
static int bcmp(const char *a,const char *b){
    while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;
}

/* ── Format ──────────────────────────────────────────────────────────────── */
int stackfs_format(const char *label){
    if(!ata_drive.present){kprintf("[stackfs] no disk\n");return -1;}
    kprintf("[stackfs] formatting...\n");
    uint32_t spb=STACKFS_BLOCK_SIZE/STACKFS_SECTOR_SIZE;
    uint32_t total=(ata_drive.sectors-STACKFS_DATA_START)/spb;
    for(size_t i=0;i<sizeof(sb);i++)((uint8_t*)&sb)[i]=0;
    sb.magic=STACKFS_MAGIC;sb.version=STACKFS_VERSION;
    sb.total_blocks=total;sb.free_blocks=total;
    sb.total_inodes=STACKFS_MAX_INODES;sb.free_inodes=STACKFS_MAX_INODES-1;
    sb.root_inode=1;bscpy(sb.label,label?label:"StackFS",32);
    for(int i=0;i<STACKFS_MAX_INODES;i++)
        for(size_t b=0;b<sizeof(stackfs_inode_t);b++)((uint8_t*)&inodes[i])[b]=0;
        for(int i=0;i<(int)sizeof(bitmap);i++)bitmap[i]=0;
        inodes[1].type=SFS_TYPE_DIR;inodes[1].perm=0755;
    bscpy(inodes[1].name,"/",STACKFS_NAME_MAX);
    sb_dirty=inodes_dirty=bitmap_dirty=1;
    stackfs_sync();
    kprintf("[stackfs] format done: %u blocks\n",total);
    return 0;
}

int stackfs_detect(void){
    if(!ata_drive.present)return 0;
    stackfs_super_t tmp;
    if(read_sector(STACKFS_SB_SECTOR,&tmp)<0)return 0;
    return(tmp.magic==STACKFS_MAGIC)?1:0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  VFS ops
 *  ═══════════════════════════════════════════════════════════════════════════ */

static int32_t bfs_read(vfs_node_t *node,uint32_t offset,
                        uint32_t size,uint8_t *buf){
    int ino=(int)node->inode;
    if(ino<=0||ino>=STACKFS_MAX_INODES)return -1;
    stackfs_inode_t *in=&inodes[ino];
    if(in->type!=SFS_TYPE_FILE)return -1;
    if(offset>=in->size)return 0;
    if(offset+size>in->size)size=in->size-offset;
    uint32_t done=0;
    while(done<size){
        uint32_t bi=(offset+done)/STACKFS_BLOCK_SIZE;
        uint32_t bo=(offset+done)%STACKFS_BLOCK_SIZE;
        uint32_t tc=STACKFS_BLOCK_SIZE-bo;
        if(tc>size-done)tc=size-done;
        if(bi>=STACKFS_DIRECT_PTRS||!in->blocks[bi])break;
        if(read_block((int)in->blocks[bi],buf_rw)<0)break;
        for(uint32_t i=0;i<tc;i++)buf[done+i]=buf_rw[bo+i];
        done+=tc;
    }
    return(int32_t)done;
                        }

                        static int32_t bfs_write(vfs_node_t *node,uint32_t offset,
                                                 uint32_t size,const uint8_t *buf){
                            int ino=(int)node->inode;
                            if(ino<=0||ino>=STACKFS_MAX_INODES)return -1;
                            stackfs_inode_t *in=&inodes[ino];
                            if(in->type!=SFS_TYPE_FILE)return -1;
                            uint32_t done=0;
                            while(done<size){
                                uint32_t bi=(offset+done)/STACKFS_BLOCK_SIZE;
                                uint32_t bo=(offset+done)%STACKFS_BLOCK_SIZE;
                                uint32_t tw=STACKFS_BLOCK_SIZE-bo;
                                if(tw>size-done)tw=size-done;
                                if(bi>=STACKFS_DIRECT_PTRS)break;
                                if(!in->blocks[bi]){
                                    int nb=bitmap_alloc();if(nb<0)break;
                                    in->blocks[bi]=(uint32_t)nb;
                                    for(int i=0;i<STACKFS_BLOCK_SIZE;i++)buf_rw[i]=0;
                                }else{
                                    read_block((int)in->blocks[bi],buf_rw);
                                }
                                for(uint32_t i=0;i<tw;i++)buf_rw[bo+i]=buf[done+i];
                                if(write_block((int)in->blocks[bi],buf_rw)<0)break;
                                done+=tw;
                            }
                            if(offset+done>in->size){in->size=offset+done;inodes_dirty=1;}
                            node->size=in->size;
                            stackfs_sync();
                            return(int32_t)done;
                                                 }

                                                 /*
                                                  * bfs_readdir — uses buf_dir (separate from buf_find used by bfs_finddir).
                                                  * The name is copied into a static dirent_t BEFORE returning.
                                                  * buf_dir is freed conceptually when the next readdir call starts.
                                                  */
                                                 static dirent_t *bfs_readdir(vfs_node_t *node,uint32_t idx){
                                                     int ino=(int)node->inode;
                                                     if(ino<=0||ino>=STACKFS_MAX_INODES)return NULL;
                                                     stackfs_inode_t *in=&inodes[ino];
                                                     if(in->type!=SFS_TYPE_DIR)return NULL;
                                                     uint32_t epb=STACKFS_BLOCK_SIZE/sizeof(stackfs_dirent_t);
                                                     uint32_t bi=idx/epb, bo=idx%epb;
                                                     if(bi>=STACKFS_DIRECT_PTRS||!in->blocks[bi])return NULL;
                                                     read_block((int)in->blocks[bi],buf_dir);
                                                     stackfs_dirent_t *de=(stackfs_dirent_t*)buf_dir+bo;
                                                     if(!de->inode||!de->name[0])return NULL;
                                                     static dirent_t result;
                                                     bscpy(result.name,de->name,VFS_NAME_MAX);
                                                     result.inode=de->inode;
                                                     /* Name is now in result.name — buf_dir can be reused by finddir */
                                                     return &result;
                                                 }

                                                 /*
                                                  * bfs_finddir — uses buf_find (separate from buf_dir used by bfs_readdir).
                                                  * Safe to call immediately after bfs_readdir.
                                                  */
                                                 static vfs_node_t *bfs_finddir(vfs_node_t *node,const char *name){
                                                     int ino=(int)node->inode;
                                                     if(ino<=0||ino>=STACKFS_MAX_INODES)return NULL;
                                                     stackfs_inode_t *in=&inodes[ino];
                                                     if(in->type!=SFS_TYPE_DIR)return NULL;
                                                     uint32_t epb=STACKFS_BLOCK_SIZE/sizeof(stackfs_dirent_t);
                                                     for(int b=0;b<STACKFS_DIRECT_PTRS&&in->blocks[b];b++){
                                                         read_block((int)in->blocks[b],buf_find);
                                                         stackfs_dirent_t *e=(stackfs_dirent_t*)buf_find;
                                                         for(uint32_t i=0;i<epb;i++){
                                                             if(!e[i].inode||!e[i].name[0])continue;
                                                             if(bcmp(e[i].name,name)==0){
                                                                 int ci=(int)e[i].inode;
                                                                 if(ci<=0||ci>=STACKFS_MAX_INODES)return NULL;
                                                                 vfs_node_t *r=(vfs_node_t*)kmalloc(sizeof(vfs_node_t));
                                                                 if(!r)return NULL;
                                                                 for(size_t x=0;x<sizeof(vfs_node_t);x++)((uint8_t*)r)[x]=0;
                                                                 bscpy(r->name,inodes[ci].name,VFS_NAME_MAX);
                                                                 r->inode=(uint32_t)ci;r->size=inodes[ci].size;
                                                                 r->flags=(inodes[ci].type==SFS_TYPE_DIR)?VFS_DIR:VFS_FILE;
                                                                 extern vfs_ops_t stackfs_ops;r->ops=&stackfs_ops;
                                                                 return r;
                                                             }
                                                         }
                                                     }
                                                     return NULL;
                                                 }

                                                 static int dir_add_entry(int dir_ino,int child_ino,const char *name){
                                                     stackfs_inode_t *dir=&inodes[dir_ino];
                                                     uint32_t epb=STACKFS_BLOCK_SIZE/sizeof(stackfs_dirent_t);
                                                     for(int b=0;b<STACKFS_DIRECT_PTRS;b++){
                                                         if(!dir->blocks[b]){
                                                             int nb=bitmap_alloc();if(nb<0)return -1;
                                                             dir->blocks[b]=(uint32_t)nb;inodes_dirty=1;
                                                             for(int i=0;i<STACKFS_BLOCK_SIZE;i++)buf_rw[i]=0;
                                                         }else{
                                                             read_block((int)dir->blocks[b],buf_rw);
                                                         }
                                                         stackfs_dirent_t *e=(stackfs_dirent_t*)buf_rw;
                                                         for(uint32_t i=0;i<epb;i++){
                                                             if(!e[i].inode){
                                                                 e[i].inode=(uint32_t)child_ino;
                                                                 bscpy(e[i].name,name,STACKFS_NAME_MAX);
                                                                 write_block((int)dir->blocks[b],buf_rw);
                                                                 return 0;
                                                             }
                                                         }
                                                     }
                                                     return -1;
                                                 }

                                                 static int bfs_create(vfs_node_t *dir,const char *name,uint32_t flags){
                                                     (void)flags;
                                                     int di=(int)dir->inode,ni=alloc_inode();
                                                     if(ni<0)return -1;
                                                     inodes[ni].type=SFS_TYPE_FILE;inodes[ni].perm=0644;inodes[ni].size=0;
                                                     bscpy(inodes[ni].name,name,STACKFS_NAME_MAX);inodes_dirty=1;
                                                     dir_add_entry(di,ni,name);stackfs_sync();return 0;
                                                 }

                                                 static int bfs_mkdir(vfs_node_t *dir,const char *name){
                                                     int di=(int)dir->inode,ni=alloc_inode();
                                                     if(ni<0)return -1;
                                                     inodes[ni].type=SFS_TYPE_DIR;inodes[ni].perm=0755;inodes[ni].size=0;
                                                     bscpy(inodes[ni].name,name,STACKFS_NAME_MAX);inodes_dirty=1;
                                                     dir_add_entry(di,ni,name);stackfs_sync();return 0;
                                                 }

                                                 static int bfs_unlink(vfs_node_t *dir,const char *name){
                                                     int di=(int)dir->inode;
                                                     stackfs_inode_t *d=&inodes[di];
                                                     uint32_t epb=STACKFS_BLOCK_SIZE/sizeof(stackfs_dirent_t);
                                                     for(int b=0;b<STACKFS_DIRECT_PTRS&&d->blocks[b];b++){
                                                         read_block((int)d->blocks[b],buf_rw);
                                                         stackfs_dirent_t *e=(stackfs_dirent_t*)buf_rw;
                                                         for(uint32_t i=0;i<epb;i++){
                                                             if(e[i].inode&&bcmp(e[i].name,name)==0){
                                                                 int ci=(int)e[i].inode;
                                                                 for(int p=0;p<STACKFS_DIRECT_PTRS;p++)
                                                                     if(inodes[ci].blocks[p])bitmap_free_blk((int)inodes[ci].blocks[p]);
                                                                     for(size_t x=0;x<sizeof(stackfs_inode_t);x++)((uint8_t*)&inodes[ci])[x]=0;
                                                                     sb.free_inodes++;sb_dirty=1;inodes_dirty=1;
                                                                 e[i].inode=0;e[i].name[0]='\0';
                                                                 write_block((int)d->blocks[b],buf_rw);
                                                                 stackfs_sync();return 0;
                                                             }
                                                         }
                                                     }
                                                     return -1;
                                                 }

                                                 vfs_ops_t stackfs_ops={
                                                     .read=bfs_read,.write=bfs_write,.open=NULL,.close=NULL,
                                                     .readdir=bfs_readdir,.finddir=bfs_finddir,
                                                     .create=bfs_create,.mkdir=bfs_mkdir,.unlink=bfs_unlink,
                                                 };

                                                 vfs_node_t *stackfs_mount(void){
                                                     if(!ata_drive.present)return NULL;
                                                     if(read_sector(STACKFS_SB_SECTOR,&sb)<0)return NULL;
                                                     if(sb.magic!=STACKFS_MAGIC){kprintf("[stackfs] bad magic\n");return NULL;}
                                                     load_inodes();load_bitmap();
                                                     sb_dirty=inodes_dirty=bitmap_dirty=0;
                                                     kprintf("[stackfs] mounted '%s'  %u/%u blocks free\n",
                                                             sb.label,sb.free_blocks,sb.total_blocks);
                                                     mounted=1;
                                                     vfs_node_t *root=(vfs_node_t*)kmalloc(sizeof(vfs_node_t));
                                                     if(!root)return NULL;
                                                     for(size_t i=0;i<sizeof(vfs_node_t);i++)((uint8_t*)root)[i]=0;
                                                     bscpy(root->name,"/",VFS_NAME_MAX);
                                                     root->flags=VFS_DIR;root->inode=(uint32_t)sb.root_inode;
                                                     root->size=0;root->ops=&stackfs_ops;return root;
                                                 }
