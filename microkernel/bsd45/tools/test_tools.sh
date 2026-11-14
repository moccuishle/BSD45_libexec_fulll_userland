#!/usr/bin/env bash
# test_tools.sh - /bin/head, /bin/tail, /bin/uname, /bin/true, /bin/false.
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }

echo "tools acceptance: head/tail/uname/true/false"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
  -Keys "shell|head -2 /readme.txt|tail -2 /readme.txt|head -1 /readme.txt|uname|uname -a|true && echo true-ok|false || echo false-ok|date|exit 0" \
  -WaitFor "user exit" -TimeoutSec 120)"
check head  "$log" "BSD45-ELF-x86  -  4.4BSD-Lite2 i386 ELF kernel"
check head2 "$log" "=============================================="
check tail  "$log" "bsd45> help"
check uname "$log" "BSD45 bsd45 1.0 4.4BSD-Lite2 i386 ELF kernel i386"
check true  "$log" "true-ok"
check false "$log" "false-ok"
if printf '%s' "$log" | tr -d '\r' | grep -qE '^(Sun|Mon|Tue|Wed|Thu|Fri|Sat) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) +[0-9]+ [0-9]{2}:[0-9]{2}:[0-9]{2} UTC [0-9]{4}'; then
  echo "  ok   date: well-formed timestamp"
else
  echo "  FAIL date: no well-formed timestamp" >&2
  fails=$((fails+1))
fi
check exit  "$log" "[user exit 0]"
printf '%s' "$log" | grep -q '\*\*\* TRAP' && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }
if [ "$fails" -ne 0 ]; then echo "tools: FAILED ($fails)" >&2; exit 1; fi
echo "tools: PASSED"
exit 0
