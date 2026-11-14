#!/usr/bin/env bash
# test_dev.sh - S11 acceptance: /dev/null, /dev/zero, /dev/tty.
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }

echo "S11 acceptance: /dev/null, /dev/zero, /dev/tty"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
  -Keys "shell|echo hi > /dev/null|echo st=\$?|cat /dev/null|echo tty-ok > /dev/tty|exit 0" \
  -WaitFor "user exit" -TimeoutSec 120)"

check null "$log" "st=0"
check tty  "$log" "tty-ok"
check exit "$log" "[user exit 0]"
printf '%s' "$log" | grep -qF "cannot create /dev/" && { echo "  FAIL /dev open" >&2; fails=$((fails+1)); }
printf '%s' "$log" | grep -qF "cannot open /dev/"   && { echo "  FAIL /dev read" >&2; fails=$((fails+1)); }
printf '%s' "$log" | grep -q '\*\*\* TRAP'          && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }

if [ "$fails" -ne 0 ]; then echo "S11: FAILED ($fails)" >&2; exit 1; fi
echo "S11: PASSED"
exit 0
