/*
 * vfs.h - a small vnode/VFS layer.
 *
 * Everything that used to be special-cased in the syscall switch (ramfs,
 * ISO9660, tmpfs) now goes through a vnode with an ops vector, so the
 * syscall code no longer knows which file system it is talking to.
 */
#ifndef _VFS_H_
#define _VFS_H_

#define	VREG	1
#define	VDIR	2

struct vnode;

struct vnodeops {
	int	(*lookup)(struct vnode *dv, const char *name,
		    struct vnode **vpp);
	int	(*getattr)(struct vnode *vp, unsigned *mode, unsigned *size);
	int	(*read)(struct vnode *vp, unsigned off, void *buf,
		    unsigned len);
	int	(*write)(struct vnode *vp, unsigned off, const void *buf,
		    unsigned len);
	int	(*readdir)(struct vnode *vp, unsigned *cookie, char *name,
		    int *isdir);
	int	(*create)(struct vnode *dv, const char *name,
		    struct vnode **vpp);
	int	(*remove)(struct vnode *dv, const char *name);
	int	(*mkdir)(struct vnode *dv, const char *name);
	int	(*rmdir)(struct vnode *dv, const char *name);
	int	(*truncate)(struct vnode *vp);
	int	(*rename)(struct vnode *dv, const char *old,
		    const char *new);
};

struct vnode {
	int			ref;
	int			type;		/* VREG or VDIR */
	struct vnodeops		*ops;
	unsigned		lba;		/* ISO: start sector */
	unsigned		size;		/* ISO: byte size */
	int			node;		/* tmpfs: table slot */
};

extern struct vnode *rootvnode;

/*
 * VFS core services available to backends (S100.2a): a backend may allocate
 * a vnode for a lookup result, but must not reach into the pool itself.
 */
struct vnode *vn_alloc(void);

/*
 * Backends (S100.2a: tmpfs lives in tmpfs.c behind these, so vfs.c stays a
 * forwarding layer and the microkernel variant can replace the backend with
 * an IPC client without touching namei()/the mount table).
 */
void	tmpfs_init(void);
struct vnode *tmpfs_root(void);
int	tmpfs_is_root(struct vnode *vp);

void	vfs_init(void);
int	vfs_namei(const char *path, struct vnode **vpp);
int	vfs_lookup_parent(const char *path, struct vnode **dvp, char *name);
int	vfs_create(const char *path, int isdir, struct vnode **vpp);
int	vfs_mount_list(char *buf, unsigned len);
int	vfs_absolutize(const char *cwd, const char *in, char *out,
	    unsigned outlen);
int	vfs_rename(const char *oldpath, const char *newpath);
void	vn_put(struct vnode *vp);
void	vfs_stats(unsigned *vtotal, unsigned *vused, unsigned *vfree,
	    unsigned *pnalloc, unsigned *pnfree, unsigned *pninuse);

#endif
