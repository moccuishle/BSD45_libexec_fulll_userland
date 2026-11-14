#!/usr/bin/env bash
# test_s10_4.sh - S10.4 acceptance (bash twin of test_s10_4.ps1)
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() { local n="$1" l="$2" x; shift 2; for x in "$@"; do
  if printf '%s' "$l" | grep -qF -- "$x"; then echo "  ok   $n: found $x"
  else echo "  FAIL $n: missing $x" >&2; fails=$((fails+1)); fi; done; }

echo "S10.4 acceptance: mount table, chdir/getcwd, relative paths"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
  -Keys "shell|cd /tmp|pwd|echo vfs-rel > a|cat a|ls|cd ..|pwd|cd /bin|ls|exit 0" \
  -WaitFor "user exit" -TimeoutSec 150)"

check cd/pwd   "$log" "/tmp"
check relredir "$log" "vfs-rel"
check cddotdot "$log" "vfs-rel"      # cd .. 后 pwd 为 /
check cd_bin   "$log" "sh.elf"
check exit     "$log" "[user exit 0]"
printf '%s' "$log" | grep -qF "cannot create" && { echo "  FAIL relative redirect" >&2; fails=$((fails+1)); }
printf '%s' "$log" | grep -q '\*\*\* TRAP'     && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }

if [ "$fails" -ne 0 ]; then echo "S10.4: FAILED ($fails)" >&2; exit 1; fi
echo "S10.4: PASSED"
exit 0
