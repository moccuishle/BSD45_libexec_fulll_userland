/*
 * chkstk_ms.s - i386 stack probe helper (__chkstk_ms).
 *
 * gcc emits `call __chkstk_ms' before setting up a stack frame larger than
 * a page, so that the guard pages below the stack get touched in order.
 * Normally libgcc provides it; the -m32 MSYS2 x86_64 gcc has no 32-bit
 * libgcc, so it lives here.  This is the classic cygwin/i386 implementation:
 * it only probes memory, the caller still adjusts %esp.
 */

	.text
	.globl	___chkstk_ms
___chkstk_ms:
	pushl	%ecx
	pushl	%eax
	cmpl	$0x1000, %eax
	leal	0x0c(%esp), %ecx
	jb	2f
1:	subl	$0x1000, %ecx
	orl	$0x0, (%ecx)
	subl	$0x1000, %eax
	cmpl	$0x1000, %eax
	ja	1b
2:	subl	%eax, %ecx
	orl	$0x0, (%ecx)
	popl	%eax
	popl	%ecx
	ret
