/*
 * quad32.c - the few 64-bit helper routines gcc emits for i386 code.
 *
 * The build uses an x86_64 MSYS2 gcc in -m32 mode, so there is no 32-bit
 * libgcc to link against: the 64-bit division helpers have to come from
 * somewhere.  4.5BSD's sh needs __divdi3 (miscbltin.c, for %lld / ulimit),
 * so implement the quad routines here using only 32-bit arithmetic (using
 * long long operators would recurse into these very functions).
 *
 * Symbol names match libgcc's: C `__divdi3' -> PE `___divdi3', which is
 * exactly what gcc emits.
 */

typedef union {
	long long v;
	struct { unsigned long lo, hi; } w;
} qval;

typedef union {
	unsigned long long v;
	struct { unsigned long lo, hi; } w;
} uqval;

static void
neg64(uqval *x)
{
	unsigned long old = x->w.lo;

	x->w.lo = 0UL - x->w.lo;
	x->w.hi = ~x->w.hi + (x->w.lo == 0 ? 1UL : 0UL);
	(void)old;
}

static void
udivmod64(uqval n, uqval d, uqval *qp, uqval *rp)
{
	uqval q, r;
	int i;

	q.w.lo = q.w.hi = 0;
	r.w.lo = r.w.hi = 0;

	if (d.w.hi == 0 && d.w.lo == 0) {	/* n / 0: keep it defined */
		*qp = q;
		*rp = r;
		return;
	}

	for (i = 63; i >= 0; i--) {
		unsigned long bit;

		bit = (i >= 32) ? ((n.w.hi >> (i - 32)) & 1UL)
				: ((n.w.lo >> i) & 1UL);
		/* r = (r << 1) | bit */
		r.w.hi = (r.w.hi << 1) | (r.w.lo >> 31);
		r.w.lo = (r.w.lo << 1) | bit;
		/* q <<= 1 */
		q.w.hi = (q.w.hi << 1) | (q.w.lo >> 31);
		q.w.lo = q.w.lo << 1;
		if (r.w.hi > d.w.hi ||
		    (r.w.hi == d.w.hi && r.w.lo >= d.w.lo)) {
			unsigned long old = r.w.lo;

			r.w.lo -= d.w.lo;
			r.w.hi = r.w.hi - d.w.hi - (r.w.lo > old ? 1UL : 0UL);
			q.w.lo |= 1UL;
		}
	}
	*qp = q;
	*rp = r;
}

unsigned long long
__udivdi3(unsigned long long a, unsigned long long b)
{
	uqval n, d, q, r;

	n.v = a; d.v = b;
	udivmod64(n, d, &q, &r);
	return (q.v);
}

unsigned long long
__umoddi3(unsigned long long a, unsigned long long b)
{
	uqval n, d, q, r;

	n.v = a; d.v = b;
	udivmod64(n, d, &q, &r);
	return (r.v);
}

long long
__divdi3(long long a, long long b)
{
	uqval n, d, q, r;
	int neg;

	n.v = (unsigned long long)a;
	d.v = (unsigned long long)b;
	neg = 0;
	if (n.w.hi & 0x80000000UL) { neg64(&n); neg ^= 1; }
	if (d.w.hi & 0x80000000UL) { neg64(&d); neg ^= 1; }
	udivmod64(n, d, &q, &r);
	if (neg)
		neg64(&q);
	return ((long long)q.v);
}

long long
__moddi3(long long a, long long b)
{
	uqval n, d, q, r;
	int neg;

	n.v = (unsigned long long)a;
	d.v = (unsigned long long)b;
	neg = 0;
	if (n.w.hi & 0x80000000UL) { neg64(&n); neg ^= 1; }
	if (d.w.hi & 0x80000000UL) { neg64(&d); }
	udivmod64(n, d, &q, &r);
	if (neg)
		neg64(&r);
	return ((long long)r.v);
}

/* 64x64 -> low 64 bits of the product, again without 64-bit operators. */
static void
umul32(unsigned long a, unsigned long b, unsigned long *hi, unsigned long *lo)
{
	unsigned long a0 = a & 0xffffUL, a1 = a >> 16;
	unsigned long b0 = b & 0xffffUL, b1 = b >> 16;
	unsigned long p00 = a0 * b0, p01 = a0 * b1;
	unsigned long p10 = a1 * b0, p11 = a1 * b1;
	unsigned long mid = (p00 >> 16) + (p01 & 0xffffUL) + (p10 & 0xffffUL);

	*lo = (p00 & 0xffffUL) | (mid << 16);
	*hi = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16);
}

unsigned long long
__muldi3(unsigned long long a, unsigned long long b)
{
	uqval x, y, z;
	unsigned long h0, l0, h1, l1, h2, l2;

	x.v = a; y.v = b;
	umul32(x.w.lo, y.w.lo, &h0, &l0);
	umul32(x.w.lo, y.w.hi, &h1, &l1);
	umul32(x.w.hi, y.w.lo, &h2, &l2);
	z.w.lo = l0;
	z.w.hi = h0 + l1 + l2;
	return (z.v);
}
