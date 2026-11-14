#!/usr/bin/env bash
# test_mount.sh - S12 acceptance: /dev/mounts + /bin/df.
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }

echo "S12 acceptance: /dev/mounts + /bin/df"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
  -Keys "shell|df|mount|cat /dev/mounts|exit 0" -WaitFor "user exit" -TimeoutSec 120)"
check header "$log" "Filesystem  Mounted on  Size"
check mount  "$log" "device on mount point type"
# The ISO root LBA depends on the mastering tool (VMware mkisofs: 24,
# MSYS2 xorriso: 19), so only assert the shape of the line, not the LBA.
check iso    "$log" "iso9660 lba="
check tmpfs  "$log" "tmpfs"
check exit   "$log" "[user exit 0]"
printf '%s' "$log" | grep -qF "cannot open" && { echo "  FAIL /dev/mounts" >&2; fails=$((fails+1)); }
printf '%s' "$log" | grep -q '\*\*\* TRAP'    && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }
if [ "$fails" -ne 0 ]; then echo "S12: FAILED ($fails)" >&2; exit 1; fi
echo "S12: PASSED"
exit 0
