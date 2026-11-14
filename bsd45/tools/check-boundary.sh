#!/usr/bin/env bash
# check-boundary.sh - S100.1 guard: FS/driver code must talk to the kernel only
# through the primitives listed in docs/kernel-boundary.md.
#
# Hard rules (must be zero):
#   kernel/fs/*.c and kernel/driver/*.c may not touch the scheduler, the process
#   table, the physical page allocator or the pmap entry points directly.
# Soft rules (known layering debt, reported but not fatal):
#   direct atapi_* calls and pmap_map() from FS code; S100.4 turns these into
#   the bdev service / vfs_page_map() wrapper.
#
# Exit status: 0 = no hard violation, 1 = hard violation, 2 = nothing to check.
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ws" || exit 1

HARD='\b(cur|procs|proc_alloc|proc_find|proc_exit|proc_sched|P_BLOCKED|page_alloc|page_free|zero_page|user_map|pmap_enter|pmap_remove)\b'
DEBT='\b(atapi_read|atapi_present|atapi_capacity|atapi_inquiry|pmap_map)\b'

files=$(ls kernel/fs/*.c kernel/driver/*.c 2>/dev/null)
if [ -z "$files" ]; then
	echo "check-boundary: no kernel/fs/*.c or kernel/driver/*.c found" >&2
	exit 2
fi

echo "S100.1 kernel boundary check"
echo "  files: $(echo "$files" | tr '\n' ' ')"
echo "  allowed: user_ok (KP-1), pmap_map/vfs_page_* (KP-2/3),"
echo "           block_on/wakeup (KP-4), inb/outb (KP-5), ipc (KP-6/7)"

hard=$(grep -nE "$HARD" $files 2>/dev/null || true)
if [ -n "$hard" ]; then
	echo ""
	echo "FAIL hard boundary violation (scheduler/proc/pmap internals):"
	echo "$hard" | sed 's/^/  /'
	echo ""
	echo "  FS/driver code must use the primitives from docs/kernel-boundary.md."
	exit 1
fi
echo "  hard rules: OK (0 violations)"

debt=$(grep -nE "$DEBT" kernel/fs/*.c 2>/dev/null | \
	grep -vE ':[0-9]+:([[:space:]]*\*|extern )' || true)
if [ -n "$debt" ]; then
	n=$(printf '%s\n' "$debt" | wc -l)
	echo "  known layering debt: $n (see docs/kernel-boundary.md section 4.2)"
	printf '%s\n' "$debt" | head -12 | sed 's/^/    /'
	[ "$n" -gt 12 ] && echo "    ..."
else
	echo "  known layering debt: 0"
fi

echo ""
echo "check-boundary: PASS (hard rules)"
exit 0
