#!/usr/bin/env bash
# test_s10_3.sh - S10.3 acceptance (bash twin of test_s10_3.ps1)
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }
echo "S10.3 acceptance: rm / cp / mv / wc / grep + rename(2) + read-only ISO"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
  -Keys "shell|cp /readme.txt /tmp/r|cat /tmp/r|wc /tmp/r|grep BSD45 /tmp/r|mv /tmp/r /tmp/r2|ls /tmp|rm /tmp/r2|ls /tmp|mkdir /tmp/x|rm -r /tmp/x|rm /readme.txt|echo st=\$?|exit 0" \
  -WaitFor "user exit" -TimeoutSec 150)"
check cp/cat "$log" "4.4BSD-Lite2 i386 ELF kernel"
printf '%s' "$log" | grep -qE '[0-9]+ [0-9]+ [0-9]+' && echo "  ok   wc: counts printed" || { echo "  FAIL wc" >&2; fails=$((fails+1)); }
check grep   "$log" "ET_EXEC"
check mv     "$log" "r2"
check rm     "$log" "Read-only file system"
check exit   "$log" "[user exit 0]"
printf '%s' "$log" | grep -q '\*\*\* TRAP' && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }
if [ "$fails" -ne 0 ]; then echo "S10.3: FAILED ($fails)" >&2; exit 1; fi
echo "S10.3: PASSED"
exit 0
