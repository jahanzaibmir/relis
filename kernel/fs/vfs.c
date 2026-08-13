/* =============================================================================
 *  RELIS — kernel/fs/vfs.c
 *  VFS core — path resolution, dispatch, mount table.
 *
 *  OWNERSHIP RULES (critical):
 *  - vfs_open() returns the EXACT mount root node for paths that match
 *    a mount point exactly (e.g. "/disk"). These must NEVER be freed.
 *  - vfs_open() returns a kmalloc'd node for sub-paths (e.g. "/disk/etc").
 *    These MUST be freed by the caller.
 *  - To tell them apart: vfs_node_is_mount_root(n) checks all mount entries.
 *
 *  The previous freeze was caused by cmd_dir calling vnode_free(d) where d
 *  was the /disk mount root. kfree(d) freed the mount root node. All
 *  subsequent vfs_open("/disk") calls followed the dangling pointer and
 *  crashed or looped forever.
 *  ============================================================================= */
#include "vfs.h"
#include "kprintf.h"
#include "mm/heap.h"
#include <stddef.h>

#define MAX_MOUNTS 16

typedef struct {
    char        path[VFS_PATH_MAX];
    vfs_node_t *node;
    int         used;
} mount_entry_t;

static mount_entry_t mounts[MAX_MOUNTS];
static vfs_node_t   *fs_root = NULL;

static size_t vstrlen(const char *s){size_t n=0;while(s[n])n++;return n;}
static int vstrcmp(const char *a,const char *b){
    while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;
}
static void vstrcpy(char *d,const char *s,size_t max){
    size_t i=0;while(i<max-1&&s[i]){d[i]=s[i];i++;}d[i]='\0';
}

void vfs_init(void){
    for(int i=0;i<MAX_MOUNTS;i++) mounts[i].used=0;
    kprintf("[vfs] initialised\n");
}

int vfs_mount(const char *path,vfs_node_t *node){
    for(int i=0;i<MAX_MOUNTS;i++){
        if(!mounts[i].used){
            vstrcpy(mounts[i].path,path,VFS_PATH_MAX);
            mounts[i].node=node;mounts[i].used=1;
            if(vstrcmp(path,"/")==0) fs_root=node;
            kprintf("[vfs] mounted '%s' at %s\n",node->name,path);
            return 0;
        }
    }
    return -1;
}

vfs_node_t *vfs_root(void){return fs_root;}

/* Check if a node is ANY mount root — if so it must never be freed */
int vfs_node_is_mount_root(vfs_node_t *node){
    if(!node) return 0;
    for(int i=0;i<MAX_MOUNTS;i++)
        if(mounts[i].used && mounts[i].node==node) return 1;
        return 0;
}

static vfs_node_t *mount_lookup(const char *path){
    for(int i=0;i<MAX_MOUNTS;i++)
        if(mounts[i].used&&vstrcmp(mounts[i].path,path)==0)
            return mounts[i].node;
    return NULL;
}

/*
 * vfs_open — walk path components, return resolved node.
 *
 * Return value ownership:
 *   - If the result IS a mount root → NOT owned by caller, do NOT free.
 *   - If the result is a sub-node   → owned by caller, MUST free.
 *
 * Callers should use vfs_node_free() which checks this automatically.
 */
vfs_node_t *vfs_open(const char *path){
    if(!path||!fs_root) return NULL;

    char norm[VFS_PATH_MAX];
    vstrcpy(norm,path,VFS_PATH_MAX);
    size_t nl=vstrlen(norm);
    if(nl>1&&norm[nl-1]=='/'){norm[nl-1]='\0';}

    /* Root */
    if(norm[0]=='/'&&norm[1]=='\0') return fs_root;

    /* Exact mount point match — return mount root directly, not a copy */
    vfs_node_t *mnt=mount_lookup(norm);
    if(mnt) return mnt;

    /* Path walk */
    vfs_node_t *cur=fs_root;
    int cur_is_mount=1;   /* fs_root is a mount root — don't free it */

    const char *p=norm;
    if(*p=='/') p++;

    char walked[VFS_PATH_MAX];
    walked[0]='/';walked[1]='\0';
    char component[VFS_NAME_MAX];

    while(*p&&cur){
        size_t len=0;
        while(p[len]&&p[len]!='/') len++;
        if(len==0){if(*p=='/')p++;continue;}
        if(len>=VFS_NAME_MAX){
            if(!cur_is_mount) kfree(cur);
            return NULL;
        }
        for(size_t i=0;i<len;i++) component[i]=p[i];
        component[len]='\0';
        p+=len;if(*p=='/') p++;

        /* Build walked path for mount lookup */
        size_t wl=vstrlen(walked);
        if(wl>1){walked[wl]='/';walked[wl+1]='\0';wl++;}
        vstrcpy(walked+wl,component,VFS_PATH_MAX-wl);

        /* Check if this walked path is a mount point */
        vfs_node_t *mp=mount_lookup(walked);
        if(mp){
            if(!cur_is_mount) kfree(cur);
            cur=mp;cur_is_mount=1;
            continue;
        }

        if(!(cur->flags&VFS_DIR)){
            if(!cur_is_mount) kfree(cur);
            return NULL;
        }

        /* Walk into child — finddir returns kmalloc'd node */
        vfs_node_t *child=vfs_finddir(cur,component);

        /* Free previous cur only if we own it */
        if(!cur_is_mount) kfree(cur);

        cur=child;
        cur_is_mount=0;   /* finddir result is owned by us */
    }

    return cur;
}

/* ── Wrappers ────────────────────────────────────────────────────────────── */
int32_t vfs_read(vfs_node_t *n,uint32_t off,uint32_t sz,uint8_t *buf){
    if(!n||!n->ops||!n->ops->read) return -1;
    return n->ops->read(n,off,sz,buf);
}
int32_t vfs_write(vfs_node_t *n,uint32_t off,uint32_t sz,const uint8_t *buf){
    if(!n||!n->ops||!n->ops->write) return -1;
    return n->ops->write(n,off,sz,buf);
}
void vfs_close(vfs_node_t *n){
    if(n&&n->ops&&n->ops->close) n->ops->close(n);
}
dirent_t *vfs_readdir(vfs_node_t *n,uint32_t idx){
    if(!n||!n->ops||!n->ops->readdir) return NULL;
    if(!(n->flags&VFS_DIR)) return NULL;
    return n->ops->readdir(n,idx);
}
vfs_node_t *vfs_finddir(vfs_node_t *n,const char *name){
    if(!n||!n->ops||!n->ops->finddir) return NULL;
    if(!(n->flags&VFS_DIR)) return NULL;
    return n->ops->finddir(n,name);
}
