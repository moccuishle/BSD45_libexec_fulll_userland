/*
 * vfs.c - vnode layer over ISO9660 (read-only root) and tmpfs (/tmp).
 *
 * The ISO backend keeps (lba, size) in the vnode and reads sectors on
 * demand; the tmpfs backend owns a small table of nodes, one page each.
 * Mount points are resolved in namei(): "/tmp" switches from the ISO root
 * to the tmpfs root.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>

#ifndef PAGE_SIZE
#define	PAGE_SIZE	4096
#endif
#ifndef VM_PROT_READ
#define	VM_PROT_READ	0x1
#define	VM_PROT_WRITE	0x2
#endif

#include "atapi.h"
#include "isofs.h"
#include "vfs.h"

extern unsigned vfs_page_alloc(void);
extern void vfs_page_free(unsigned pa);
extern void vfs_page_unmap(unsigned va);
extern void vfs_page_stats(unsigned *, unsigned *, unsigned *);
extern void pmap_map(vm_offset_t, vm_offset_t, vm_offset_t, int);
extern int user_ok(unsigned, unsigned);
extern void bcopy(const void *, void *, unsigned);
extern void bzero(void *, unsigned);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, unsigned);
extern char *strchr(const char *, int);
extern unsigned strlen(const char *);

#define	TMPFS_MAXNODES	32
#define	TMPFS_NAMELEN	32
#define	TMPFS_DATASZ	4096
#define	TMPFS_VA	0xFE600000

#define	ISO_SECTOR	2048

struct tmpfs_node {
	int		used;
	int		isdir;
	char		name[TMPFS_NAMELEN];
	unsigned char	*data;		/* kernel window VA, 0 = no page */
	unsigned	pa;		/* backing physical page */
	unsigned	size;
};

static struct tmpfs_node tmpfs_nodes[TMPFS_MAXNODES];
static int	tmpfs_inited;

/*
 * tmpfs data pages are allocated lazily on the first write and handed back
 * by truncate/remove/rmdir, so "echo x > /tmp/f" in a loop reuses one page
 * instead of eating the whole pool.
 */
static unsigned char *
tmpfs_getdata(struct tmpfs_node *n, int create)
{
	int i = n - tmpfs_nodes;

	if (n->data != 0)
		return (n->data);
	if (!create)
		return (0);
	n->pa = vfs_page_alloc();
	n->data = (unsigned char *)(TMPFS_VA + i * PAGE_SIZE);
	pmap_map((vm_offset_t)n->data, (vm_offset_t)n->pa,
	    (vm_offset_t)n->pa + PAGE_SIZE, VM_PROT_READ | VM_PROT_WRITE);
	bzero(n->data, TMPFS_DATASZ);
	return (n->data);
}

static void
tmpfs_putdata(struct tmpfs_node *n)
{

	if (n->data == 0)
		return;
	/* drop the kernel window mapping first, then recycle the page */
	vfs_page_unmap((unsigned)n->data);
	vfs_page_free(n->pa);
	n->data = 0;
	n->pa = 0;
	n->size = 0;
}

struct vnodeops tmpfs_vnodeops;
struct vnodeops iso_vnodeops;

static struct vnode iso_rootv, tmp_root;
struct vnode *rootvnode = &iso_rootv;

static unsigned char iso_sec[ISO_SECTOR];

/* ---------------------------------------------------------------- helpers */

#define	VNODES		96
static struct vnode vnode_pool[VNODES];
static int vnode_used[VNODES];
static int vnode_free[VNODES];		/* LIFO of reusable slots */
static int vnode_nfree;
static unsigned vn_alloc_total, vn_reuse_total;

static void
vn_init(void)
{
	int i;

	vnode_nfree = 0;
	for (i = VNODES - 1; i >= 0; i--) {
		vnode_used[i] = 0;
		vnode_free[vnode_nfree++] = i;
	}
}

static struct vnode *
vn_alloc(void)
{
	int i;

	for (i = 0; i < VNODES; i++) {
		if (vnode_used[i])
			continue;
		vnode_used[i] = 1;
		vn_alloc_total++;
		bzero(&vnode_pool[i], sizeof(vnode_pool[i]));
		vnode_pool[i].ref = 1;
		vnode_pool[i].node = -1;
		return (&vnode_pool[i]);
	}
	return (0);
}

void
vn_put(struct vnode *vp)
{

	if (vp == 0 || vp == &iso_rootv || vp == &tmp_root)
		return;
	if (--vp->ref <= 0) {
		int i = vp - vnode_pool;

		vp->ref = 0;
		if (i >= 0 && i < VNODES)
			vnode_used[i] = 0;
	}
}

void
vfs_stats(unsigned *vtotal, unsigned *vused, unsigned *vfree,
    unsigned *pnalloc, unsigned *pnfree, unsigned *pninuse)
{

	int i, used = 0;

	for (i = 0; i < VNODES; i++)
		used += vnode_used[i];
	*vtotal = VNODES;
	*vused = used;
	*vfree = VNODES - used;
	vfs_page_stats(pnalloc, pnfree, pninuse);
}

/* ---------------------------------------------------------------- tmpfs */

static struct tmpfs_node *
tmpfs_find(const char *name)
{
	int i;

	for (i = 0; i < TMPFS_MAXNODES; i++)
		if (tmpfs_nodes[i].used &&
		    strcmp(tmpfs_nodes[i].name, name) == 0)
			return (&tmpfs_nodes[i]);
	return (0);
}

static struct tmpfs_node *
tmpfs_create_node(const char *name, int isdir)
{
	struct tmpfs_node *n;
	int i, len;

	len = strlen(name);
	if (len == 0 || len >= TMPFS_NAMELEN)
		return (0);
	if ((n = tmpfs_find(name)) != 0)
		return (n->isdir == isdir ? n : 0);
	for (i = 0; i < TMPFS_MAXNODES; i++) {
		if (tmpfs_nodes[i].used)
			continue;
		n = &tmpfs_nodes[i];
		bzero(n, sizeof(*n));
		n->used = 1;
		n->isdir = isdir;
		bcopy(name, n->name, len + 1);
		/* the data page is created on the first write */
		return (n);
	}
	return (0);
}

static int
tmpfs_lookup_op(struct vnode *dv, const char *name, struct vnode **vpp)
{
	struct tmpfs_node *n;
	struct vnode *vp;

	if (dv != &tmp_root)
		return (-ENOENT);
	n = tmpfs_find(name);
	if (n == 0)
		return (-ENOENT);
	vp = vn_alloc();
	if (vp == 0)
		return (-ENOMEM);
	vp->type = n->isdir ? VDIR : VREG;
	vp->ops = &tmpfs_vnodeops;
	vp->node = n - tmpfs_nodes;
	*vpp = vp;
	return (0);
}

static int
tmpfs_getattr_op(struct vnode *vp, unsigned *mode, unsigned *size)
{
	struct tmpfs_node *n;

	if (vp->node < 0) {		/* the tmpfs root */
		*mode = 0040755;
		*size = 0;
		return (0);
	}
	n = &tmpfs_nodes[vp->node];
	*mode = n->isdir ? 0040755 : 0100644;
	*size = n->isdir ? 0 : n->size;
	return (0);
}

static int
tmpfs_read_op(struct vnode *vp, unsigned off, void *buf, unsigned len)
{
	struct tmpfs_node *n;

	if (vp->node < 0)
		return (-EISDIR);
	n = &tmpfs_nodes[vp->node];
	if (n->isdir)
		return (-EISDIR);
	if (n->data == 0 || off >= n->size)
		return (0);
	if (len > n->size - off)
		len = n->size - off;
	bcopy(n->data + off, buf, len);
	return ((int)len);
}

static int
tmpfs_write_op(struct vnode *vp, unsigned off, const void *buf, unsigned len)
{
	struct tmpfs_node *n;

	if (vp->node < 0)
		return (-EISDIR);
	n = &tmpfs_nodes[vp->node];
	if (n->isdir)
		return (-EISDIR);
	if (off >= TMPFS_DATASZ)
		return (-ENOSPC);
	if (len > TMPFS_DATASZ - off)
		len = TMPFS_DATASZ - off;
	if (tmpfs_getdata(n, 1) == 0)
		return (-ENOSPC);
	bcopy(buf, n->data + off, len);
	if (off + len > n->size)
		n->size = off + len;
	return ((int)len);
}

static int
tmpfs_readdir_op(struct vnode *vp, unsigned *cookie, char *name, int *isdir)
{
	unsigned i;

	for (i = *cookie; i < TMPFS_MAXNODES; i++) {
		if (!tmpfs_nodes[i].used)
			continue;
		*cookie = i + 1;
		bcopy(tmpfs_nodes[i].name, name,
		    strlen(tmpfs_nodes[i].name) + 1);
		*isdir = tmpfs_nodes[i].isdir;
		return (0);
	}
	*cookie = TMPFS_MAXNODES;
	return (1);
}

static int
tmpfs_create_op(struct vnode *dv, const char *name, struct vnode **vpp)
{
	struct tmpfs_node *n;
	struct vnode *vp;

	if (dv != &tmp_root)
		return (-ENOENT);
	n = tmpfs_create_node(name, 0);
	if (n == 0)
		return (-ENOSPC);
	vp = vn_alloc();
	if (vp == 0)
		return (-ENOMEM);
	vp->type = VREG;
	vp->ops = &tmpfs_vnodeops;
	vp->node = n - tmpfs_nodes;
	*vpp = vp;
	return (0);
}

static int
tmpfs_remove_op(struct vnode *dv, const char *name)
{
	struct tmpfs_node *n;

	if (dv != &tmp_root)
		return (-ENOENT);
	n = tmpfs_find(name);
	if (n == 0 || n->isdir)
		return (-ENOENT);
	tmpfs_putdata(n);
	n->used = 0;
	return (0);
}

static int
tmpfs_mkdir_op(struct vnode *dv, const char *name)
{

	if (dv != &tmp_root)
		return (-ENOENT);
	if (tmpfs_find(name) != 0)
		return (-EEXIST);
	return (tmpfs_create_node(name, 1) ? 0 : -ENOSPC);
}

static int
tmpfs_truncate_op(struct vnode *vp)
{
	struct tmpfs_node *n = &tmpfs_nodes[vp->node];

	if (n->isdir)
		return (-EISDIR);
	tmpfs_putdata(n);
	return (0);
}

static int
tmpfs_rmdir_op(struct vnode *dv, const char *name)
{
	struct tmpfs_node *n;

	if (dv != &tmp_root)
		return (-ENOENT);
	n = tmpfs_find(name);
	if (n == 0 || !n->isdir)
		return (-ENOENT);
	tmpfs_putdata(n);
	n->used = 0;
	return (0);
}

static int
tmpfs_rename_op(struct vnode *dv, const char *old, const char *new)
{
	struct tmpfs_node *n;
	int len;

	if (dv != &tmp_root)
		return (-EROFS);
	n = tmpfs_find(old);
	if (n == 0)
		return (-ENOENT);
	if (tmpfs_find(new) != 0)
		return (-EEXIST);
	len = strlen(new);
	if (len == 0 || len >= TMPFS_NAMELEN)
		return (-ENAMETOOLONG);
	bzero(n->name, TMPFS_NAMELEN);
	bcopy(new, n->name, len + 1);
	return (0);
}
/* ---------------------------------------------------------------- ISO9660 */

static int
iso_scan(struct vnode *dv, const char *name, struct vnode **vpp)
{
	char nm[48];
	unsigned lba, size, pos = 0;
	int isdir, rv;

	while ((rv = iso_readdir(dv->lba, dv->size, &pos, nm, &lba, &size,
	    &isdir)) == 0) {
		if (strcmp(nm, name) == 0) {
			struct vnode *vp = vn_alloc();

			if (vp == 0)
				return (-ENOMEM);
			vp->type = isdir ? VDIR : VREG;
			vp->ops = &iso_vnodeops;
			vp->lba = lba;
			vp->size = size;
			*vpp = vp;
			return (0);
		}
	}
	return (rv < 0 ? -EIO : -ENOENT);
}

static int
iso_lookup_op(struct vnode *dv, const char *name, struct vnode **vpp)
{
	char want[48];
	int rv;

	if (dv->type != VDIR)
		return (-ENOTDIR);
	rv = iso_scan(dv, name, vpp);
	if (rv == -ENOENT && strchr(name, '.') == 0) {
		unsigned n = strlen(name);

		if (n + 5 < sizeof(want)) {
			bcopy(name, want, n);
			bcopy(".elf", want + n, 5);
			rv = iso_scan(dv, want, vpp);
		}
	}
	return (rv);
}

static int
iso_getattr_op(struct vnode *vp, unsigned *mode, unsigned *size)
{

	*mode = vp->type == VDIR ? 0040755 : 0100755;
	*size = vp->size;
	return (0);
}

static int
iso_read_op(struct vnode *vp, unsigned off, void *buf, unsigned len)
{
	unsigned done = 0;
	char *p = (char *)buf;

	if (vp->type == VDIR)
		return (-EISDIR);
	if (off >= vp->size)
		return (0);
	if (len > vp->size - off)
		len = vp->size - off;
	while (done < len) {
		unsigned pos = off + done;
		unsigned secoff = pos % ISO_SECTOR;
		unsigned n = ISO_SECTOR - secoff;

		if (n > len - done)
			n = len - done;
		if (atapi_read(vp->lba + pos / ISO_SECTOR, 1, iso_sec) < 0)
			return (-EIO);
		bcopy(iso_sec + secoff, p + done, n);
		done += n;
	}
	return ((int)done);
}

static int
iso_readdir_op(struct vnode *vp, unsigned *cookie, char *name, int *isdir)
{
	unsigned lba, size;
	int rv;

	if (vp->type != VDIR)
		return (-ENOTDIR);
	rv = iso_readdir(vp->lba, vp->size, cookie, name, &lba, &size,
	    isdir);
	return (rv < 0 ? -EIO : rv);
}

/* ---------------------------------------------------------------- ops */

struct vnodeops iso_vnodeops = {
	iso_lookup_op, iso_getattr_op, iso_read_op, 0, iso_readdir_op,
	0, 0, 0, 0, 0
};

struct vnodeops tmpfs_vnodeops = {
	tmpfs_lookup_op, tmpfs_getattr_op, tmpfs_read_op, tmpfs_write_op,
	tmpfs_readdir_op, tmpfs_create_op, tmpfs_remove_op,
	tmpfs_mkdir_op, tmpfs_rmdir_op, tmpfs_truncate_op,
	tmpfs_rename_op
};

/* ---------------------------------------------------------------- namei */

void
vfs_init(void)
{
	if (tmpfs_inited)
		return;
	tmpfs_inited = 1;
	vn_init();
	bzero(&iso_rootv, sizeof(iso_rootv));
	iso_rootv.ref = 1;
	iso_rootv.type = VDIR;
	iso_rootv.ops = &iso_vnodeops;
	iso_rootv.lba = iso_root();
	iso_rootv.size = iso_root_size();
	bzero(&tmp_root, sizeof(tmp_root));
	tmp_root.ref = 1;
	tmp_root.type = VDIR;
	tmp_root.ops = &tmpfs_vnodeops;
	tmp_root.node = -1;
	rootvnode = &iso_rootv;
}

/*
 * Resolve an absolute path.  "/tmp" (and anything under it) is a mount of
 * the tmpfs root; everything else lives on the ISO9660 image.
 */
int
vfs_namei(const char *path, struct vnode **vpp)
{
	struct vnode *vp = rootvnode;
	const char *p = path;
	int rv;

	while (*p == '/')
		p++;
	if (*p == 0) {
		*vpp = vp;
		return (0);
	}
	for (;;) {
		char name[48];
		const char *slash = p;
		int n;
		struct vnode *next;

		while (*slash && *slash != '/')
			slash++;
		n = slash - p;
		if (n == 0 || n >= (int)sizeof(name))
			return (-ENOENT);
		bcopy(p, name, n);
		name[n] = 0;

		if (vp == rootvnode && strcmp(name, "tmp") == 0)
			next = &tmp_root;
		else {
			rv = vp->ops->lookup(vp, name, &next);
			if (rv < 0)
				return (rv);
		}
		if (vp != rootvnode && vp != &tmp_root)
			vn_put(vp);
		vp = next;
		if (*slash == 0) {
			*vpp = vp;
			return (0);
		}
		p = slash;
		while (*p == '/')
			p++;
	}
}

/*
 * Split path into the parent directory vnode and the last component, used
 * by creation/removal.
 */
int
vfs_lookup_parent(const char *path, struct vnode **dvp, char *name)
{
	char dir[128];
	const char *slash = 0, *p = path;
	int n;

	while (*p) {
		if (*p == '/')
			slash = p;
		p++;
	}
	if (slash == 0)
		return (-ENOENT);
	n = slash - path;
	if (n >= (int)sizeof(dir) - 1)
		return (-ENOENT);
	if (n == 0) {
		/* "/name": the parent is the root directory */
		dir[0] = '/';
		dir[1] = 0;
	} else {
		bcopy(path, dir, n);
		dir[n] = 0;
	}
	n = strlen(slash + 1);
	if (n == 0 || n >= 48)
		return (-ENOENT);
	bcopy(slash + 1, name, n + 1);
	return (vfs_namei(dir, dvp));
}

int
vfs_create(const char *path, int isdir, struct vnode **vpp)
{
	struct vnode *dv;
	char name[48];
	int rv;

	rv = vfs_lookup_parent(path, &dv, name);
	if (rv < 0)
		return (rv);
	if (!dv->ops->create && !dv->ops->mkdir) {
		vn_put(dv);
		return (-EROFS);
	}
	if (isdir) {
		rv = dv->ops->mkdir ? dv->ops->mkdir(dv, name) : -EROFS;
		if (rv == 0)
			rv = dv->ops->lookup(dv, name, vpp);
	} else {
		rv = dv->ops->create ? dv->ops->create(dv, name, vpp) :
		    -EROFS;
	}
	vn_put(dv);
	return (rv);
}

/*
 * Rename within one directory.  The ISO9660 root is read-only, so only
 * the tmpfs mount can do this; mv(1) falls back to cp+unlink otherwise.
 */
int
vfs_rename(const char *oldpath, const char *newpath)
{
	struct vnode *odv, *ndv;
	char oname[48], nname[48];
	int rv;

	rv = vfs_lookup_parent(oldpath, &odv, oname);
	if (rv < 0)
		return (rv);
	rv = vfs_lookup_parent(newpath, &ndv, nname);
	if (rv < 0) {
		vn_put(odv);
		return (rv);
	}
	if (odv != ndv || odv->ops->rename == 0)
		rv = -EXDEV;
	else
		rv = odv->ops->rename(odv, oname, nname);
	vn_put(odv);
	vn_put(ndv);
	return (rv);
}