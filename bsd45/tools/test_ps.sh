#!/usr/bin/env bash
# test_ps.sh - S13 acceptance: /dev/ps + /bin/ps.
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }
echo "S13 acceptance: /dev/ps + /bin/ps"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" -Keys "shell|ps|cat /dev/ps|exit 0" -WaitFor "user exit" -TimeoutSec 120)"
check header "$log" "PID PPID PGRP STAT COMM"
check shell  "$log" "1 0 1 BLOCK sh"
check self   "$log" "2 1 2 RUN ps"
check exit   "$log" "[user exit 0]"
printf '%s' "$log" | grep -q '\*\*\* TRAP' && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }
if [ "$fails" -ne 0 ]; then echo "S13: FAILED ($fails)" >&2; exit 1; fi
echo "S13: PASSED"
exit 0
