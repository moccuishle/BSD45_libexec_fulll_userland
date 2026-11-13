/*
 * mon.c - on-board monitor for the stand-alone 4.4BSD-Lite2 i386 kernel.
 *
 * The monitor is not part of 4.4BSD; it is the "userland" of this
 * bring-up kernel, exercising the genuine pmap/VM code from the source
 * tree.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>

#include "isofs.h"
#include "vfs.h"

extern vm_offset_t avail_start, avail_end, virtual_avail, virtual_end;
extern int KPTphys;
extern int Maxmem, physmem;
extern void start_userland(const char *script);
extern void start_prog(const char *path, const char *script);
extern int IdlePTD;
extern char etext[], edata[], end[];

static char line[128];

static int
getline(void)
{
	int n = 0, c;

	for (;;) {
		c = cngetc();
		if (c == 0)
			continue;
		if (c == '\r' || c == '\n') {
			cnputc('\n');
			line[n] = 0;
			return (n);
		}
		if (c == '\b' || c == 0x7f) {
			if (n > 0) {
				n--;
				printf("\b \b");
			}
			continue;
		}
		if (c >= ' ' && c < 0x7f && n < (int)sizeof(line) - 1) {
			line[n++] = c;
			cnputc(c);
		}
	}
}

static int
hexval(int c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return (-1);
}

static int
parsehex(const char *s, unsigned *vp)
{
	unsigned v = 0;
	int n = 0, d;

	while (*s == ' ')
		s++;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;
	while ((d = hexval(*s)) >= 0) {
		v = (v << 4) | d;
		s++;
		n++;
	}
	*vp = v;
	return (n);
}

static void
cmd_mem(void)
{
	printf("Maxmem    %d pages  (%dK)\n", Maxmem, ctob(Maxmem));
	printf("physmem   %d pages  (%dK)\n", physmem, ctob(physmem));
	printf("avail     %x - %x\n", avail_start, avail_end);
	printf("virt      %x - %x\n", virtual_avail, virtual_end);
	printf("kernel    text %x data %x end %x\n", etext, edata, end);
}

static void
cmd_pte(const char *arg)
{
	unsigned va;
	struct pte *pte;

	if (parsehex(arg, &va) == 0) {
		printf("usage: pte <hex-address>\n");
		return;
	}
	pte = pmap_pte(kernel_pmap, (vm_offset_t)va);
	if (pte == NULL) {
		printf("%x: no page table\n", va);
		return;
	}
	printf("%x: pte %x pfn %x%s%s%s\n", va, *(int *)pte,
	    *(int *)pte >> 12,
	    (*(int *)pte & PG_V) ? " V" : "",
	    (*(int *)pte & PG_RW) ? " RW" : " RO",
	    (*(int *)pte & PG_U) ? " U" : " K");
}

/*
 * cmd_vstat - S10.1: vnode pool + physical page accounting.  Run it before
 * and after a stress loop: "vnode used" must come back down and
 * "pages alloc" must not keep growing.
 */
static void
cmd_vstat(void)
{
	unsigned vt, vu, vf, pa, pf, pi;

	vfs_stats(&vt, &vu, &vf, &pa, &pf, &pi);
	printf("vstat: vnode total %d used %d free %d\n", vt, vu, vf);
	printf("vstat: pages alloc %d freed %d inuse %d\n", pa, pf, pi);
}

/*
 * cmd_vstress - S10.1 self test: run VFS-heavy loops inside the kernel and
 * print the pool counters before/after.  If vnodes or pages leak, "used"
 * and "inuse" keep climbing instead of returning to the baseline.
 */
static void
mkpath(char *buf, const char *prefix, int n)
{
	char *p = buf;

	while (*prefix)
		*p++ = *prefix++;
	if (n >= 100)
		*p++ = '0' + (n / 100) % 10;
	if (n >= 10)
		*p++ = '0' + (n / 10) % 10;
	*p++ = '0' + n % 10;
	*p = 0;
}

static void
show_vstat(const char *tag)
{
	unsigned vt, vu, vf, pa, pf, pi;

	vfs_stats(&vt, &vu, &vf, &pa, &pf, &pi);
	printf("vstat[%s]: vnode total %d used %d free %d\n", tag, vt, vu, vf);
	printf("vstat[%s]: pages alloc %d freed %d inuse %d\n",
	    tag, pa, pf, pi);
}

static void
cmd_vstress(void)
{
	struct vnode *vp, *dv;
	char path[32], name[48], buf[64];
	int i, ok;

	show_vstat("before");

	/* 100x ISO lookup + read + vn_put */
	ok = 0;
	for (i = 0; i < 100; i++) {
		if (vfs_namei("/readme.txt", &vp) == 0) {
			if (vp->ops->read)
				vp->ops->read(vp, 0, buf, sizeof(buf));
			vn_put(vp);
			ok++;
		}
	}
	printf("vstress: iso read 100x ok=%d\n", ok);

	/* 100x tmpfs mkdir + rmdir */
	ok = 0;
	for (i = 0; i < 100; i++) {
		mkpath(path, "/tmp/s", i);
		if (vfs_create(path, 1, &vp) == 0) {
			vn_put(vp);
			if (vfs_lookup_parent(path, &dv, name) == 0) {
				if (dv->ops->rmdir && dv->ops->rmdir(dv, name) == 0)
					ok++;
				vn_put(dv);
			}
		}
	}
	printf("vstress: tmpfs mkdir/rmdir 100x ok=%d\n", ok);

	/* 100x rewrite the same tmpfs file (truncate must free the old page) */
	ok = 0;
	for (i = 0; i < 100; i++) {
		if (vfs_create("/tmp/v", 0, &vp) == 0) {
			if (vp->ops->truncate)
				vp->ops->truncate(vp);
			if (vp->ops->write &&
			    vp->ops->write(vp, 0, "x", 1) == 1)
				ok++;
			vn_put(vp);
		}
	}
	printf("vstress: tmpfs rewrite 100x ok=%d\n", ok);
	if (vfs_lookup_parent("/tmp/v", &dv, name) == 0) {
		if (dv->ops->remove)
			dv->ops->remove(dv, name);
		vn_put(dv);
	}

	show_vstat("after");
}

static void
cmd_map(void)
{
	void *p;
	int i;

	p = pmap_bootstrap_alloc(PAGE_SIZE);
	if (p == NULL) {
		printf("pmap_bootstrap_alloc failed\n");
		return;
	}
	outb(0xE9, 'm');
	printf("mapped page at %p\n", p);
	printf("pte new page: %x\n", *(int *)pmap_pte(kernel_pmap, (vm_offset_t)p));
	((int *)p)[0] = 0x4f534442;	/* "BSDO" */
	((int *)p)[1023] = 0x444e4521;
	printf("readback %x %x\n", ((int *)p)[0], ((int *)p)[1023]);
	printf("ok\n");
}

void
reboot_now(void)
{
	int i;

	printf("rebooting...\n");
	for (i = 0; i < 100000; i++)
		;
	outb(0x64, 0xfe);
	for (i = 0; i < 100000; i++)
		;
	*(int *)0 = 0;		/* triple fault fallback */
	for (;;)
		;
}

void
monitor(void)
{
	printf("BSD/386 monitor - type 'help' for commands\n");
#ifdef S10_AUTOTEST
	/* verification build only: run the S10.1 stress without a keyboard */
	cmd_vstress();
#endif

	for (;;) {
		printf("bsd45> ");
		if (getline() == 0)
			continue;

		if (!strcmp(line, "help"))
			printf("help  mem  vstat  vstress  pte <addr>  map  banner  "
			    "usermode  shell  test  cd  reboot  halt\n");
		else if (!strcmp(line, "mem"))
			cmd_mem();
		else if (!strcmp(line, "vstat"))
			cmd_vstat();
		else if (!strcmp(line, "vstress"))
			cmd_vstress();
		else if (!strncmp(line, "pte", 3))
			cmd_pte(line + 3);
		else if (!strcmp(line, "usermode"))
			start_userland("echo hello; exit 3");
		else if (!strcmp(line, "shell"))
			start_userland(0);
		else if (!strcmp(line, "cd")) {
			if (iso_mounted())
				printf("ISO9660 root: lba %d, %d bytes\n",
				    iso_root(), iso_root_size());
			else
				printf("no ISO9660 root mounted\n");
		}
		else if (!strcmp(line, "test"))
			start_prog("/test", 0);
		else if (!strcmp(line, "map"))
			cmd_map();
		else if (!strcmp(line, "banner"))
			printf("4.5BSD-Lite2 i386 ELF kernel, KERNBASE %x\n",
			    KERNBASE);
		else if (!strcmp(line, "reboot"))
			reboot_now();
		else if (!strcmp(line, "halt"))
			for (;;)
				;
		else if (line[0])
			printf("unknown command: %s\n", line);
	}
}
