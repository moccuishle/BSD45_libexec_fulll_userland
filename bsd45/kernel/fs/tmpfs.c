/*
 * tmpfs.c - the writable /tmp backend, split out of vfs.c (S100.2a).
 *
 * This backend talks to the rest of the kernel only through the primitives
 * listed in docs/kernel-boundary.md:
 *	KP-2  pmap_map()            - map a data page into the kernel window
 *	KP-3  vfs_page_alloc/free/unmap - data page ownership
 * plus bcopy/bzero and the VFS vnode helpers vn_alloc()/vn_put().
 * It never touches the process table, the scheduler or the page allocator.
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

#include "vfs.h"

extern unsigned vfs_page_alloc(void);
extern void vfs_page_free(unsigned pa);
extern void vfs_page_unmap(unsigned va);
extern void pmap_map(vm_offset_t, vm_offset_t, vm_offset_t, int);
extern void bcopy(const void *, void *, unsigned);
extern void bzero(void *, unsigned);
extern int strcmp(const char *, const char *);
extern unsigned strlen(const char *);

#define	TMPFS_MAXNODES	32
#define	TMPFS_NAMELEN	32
#define	TMPFS_DATASZ	4096
#define	TMPFS_VA	0xFE600000

struct tmpfs_node {
	int		used;
	int		isdir;
	char		name[TMPFS_NAMELEN];
	unsigned char	*data;		/* kernel window VA, 0 = no page */
	unsigned	pa;		/* backing physical page */
	unsigned	size;
};

static struct tmpfs_node tmpfs_nodes[TMPFS_MAXNODES];
static struct vnode tmpfs_rootv;

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

/* ---------------------------------------------------------------- root */

void
tmpfs_init(void)
{

	bzero(&tmpfs_rootv, sizeof(tmpfs_rootv));
	tmpfs_rootv.ref = 1;
	tmpfs_rootv.type = VDIR;
	tmpfs_rootv.ops = &tmpfs_vnodeops;
	tmpfs_rootv.node = -1;
}

struct vnode *
tmpfs_root(void)
{

	return (&tmpfs_rootv);
}

int
tmpfs_is_root(struct vnode *vp)
{

	return (vp == &tmpfs_rootv);
}

/* ---------------------------------------------------------------- nodes */

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

/* ---------------------------------------------------------------- vnodeops */

static int
tmpfs_lookup_op(struct vnode *dv, const char *name, struct vnode **vpp)
{
	struct tmpfs_node *n;
	struct vnode *vp;

	if (dv != &tmpfs_rootv)
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

	(void)vp;
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

	if (dv != &tmpfs_rootv)
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

	if (dv != &tmpfs_rootv)
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

	if (dv != &tmpfs_rootv)
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

	if (dv != &tmpfs_rootv)
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

	if (dv != &tmpfs_rootv)
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

struct vnodeops tmpfs_vnodeops = {
	tmpfs_lookup_op, tmpfs_getattr_op, tmpfs_read_op, tmpfs_write_op,
	tmpfs_readdir_op, tmpfs_create_op, tmpfs_remove_op,
	tmpfs_mkdir_op, tmpfs_rmdir_op, tmpfs_truncate_op,
	tmpfs_rename_op
};
