/*
 * hostcrt.c - minimal CRT bits for 32-bit host helper programs that are
 * linked with plain ld (no mingw32 crt/libgcc available in this install).
 *
 * genassym must run as a 32-bit program because the struct offsets it
 * prints depend on the i386 ABI.  gcc calls __main from main() to run
 * static constructors; there are none here.
 */

void
__main(void)
{
}
