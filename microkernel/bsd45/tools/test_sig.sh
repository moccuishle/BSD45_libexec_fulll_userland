#!/usr/bin/env bash
# test_sig.sh - S10.2 acceptance: signals + console job control (bash twin of test_sig.ps1)
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() {
    local name="$1" log="$2" n; shift 2
    for n in "$@"; do
        if printf '%s' "$log" | grep -qF -- "$n"; then echo "  ok   $name: found $n"
        else echo "  FAIL $name: missing $n" >&2; fails=$((fails + 1)); fi
    done
}
CTRLC=$(printf '\003')
echo "S10.2 acceptance: signals + console job control"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" -Keys "test" -WaitFor "user exit" -TimeoutSec 150)"
check handler "$log" "sig: handler ran" "sig: handler count=1"
check ignore  "$log" "sig: ignore ok"
check mask    "$log" "sig: blocked count=0" "sig: unblocked count=1"
check default "$log" "sig: term signaled=1 sig=15"
check exit    "$log" "sig: ignore-child exited=1 code=7" "[user exit 0]"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" -Keys "shell|/bin/cat|$CTRLC|echo st=\$?|exit 0" -WaitFor "user exit" -TimeoutSec 90)"
check ctrlc   "$log" "st=130"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" -Keys "shell|sleep 5 &|jobs|kill -l|exit 0" -WaitFor "user exit" -TimeoutSec 90)"
check jobs    "$log" "sleep 5"
check kill    "$log" "TERM 15" "KILL 9"
printf '%s' "$log" | grep -q '\*\*\* TRAP' && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }
if [ "$fails" -ne 0 ]; then echo "S10.2: FAILED ($fails check(s))" >&2; exit 1; fi
echo "S10.2: PASSED"
exit 0
