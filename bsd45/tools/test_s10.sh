#!/usr/bin/env bash
# test_s10.sh - S10.1 acceptance: physical page + vnode recycling.
#
#   vstress - VFS loops inside the kernel, prints pool counters before/after
#   test    - the user-space /test binary does the same through syscalls
set -uo pipefail
ws="$(cd "$(dirname "$0")/.." && pwd)"
iso="${1:-$ws/out/BSD45-ELF-x86.iso}"
[ -f "$iso" ] || { echo "ISO not found: $iso" >&2; exit 1; }
fails=0
check() {   # check <name> <log> <needle...>
    local name="$1" log="$2" n
    shift 2
    for n in "$@"; do
        if printf '%s' "$log" | grep -qF -- "$n"; then
            echo "  ok   $name: found $n"
        else
            echo "  FAIL $name: missing $n" >&2
            fails=$((fails + 1))
        fi
    done
}

echo "S10.1 acceptance: page + vnode recycling"
log="$("$ws/tools/run_qemu_test.sh" -Iso "$iso" \
    -Keys "vstat|vstress|test" -WaitFor "user exit" -TimeoutSec 150)"

check "kernel vstat" "$log" "vstat[before]: vnode total 96 used" "vstat[before]: pages alloc"
check "iso loop"     "$log" "vstress: iso read 100x ok=100"
check "tmpfs loop"   "$log" "vstress: tmpfs mkdir/rmdir 100x ok=100"
check "rewrite loop" "$log" "vstress: tmpfs rewrite 100x ok=100"
check "user space"   "$log" "s10: iso open/read/close 50x bad=0" \
                             "s10: tmpfs mkdir/rmdir 50x bad=0" \
                             "s10: tmpfs rewrite 50x bad=0"
check "exit"         "$log" "[user exit 0]"

num() {   # num <tag> <field-regex> -> number
    printf '%s' "$log" | sed -n "s/.*vstat\[$1\]: $2.*/\1/p" | head -1
}
b_vused=$(printf '%s' "$log" | sed -n 's/.*vstat\[before\]: vnode total [0-9]* used \([0-9]*\).*/\1/p' | head -1)
a_vused=$(printf '%s' "$log" | sed -n 's/.*vstat\[after\]: vnode total [0-9]* used \([0-9]*\).*/\1/p' | head -1)
a_palloc=$(printf '%s' "$log" | sed -n 's/.*vstat\[after\]: pages alloc \([0-9]*\).*/\1/p' | head -1)
a_pfreed=$(printf '%s' "$log" | sed -n 's/.*vstat\[after\]: pages alloc [0-9]* freed \([0-9]*\).*/\1/p' | head -1)
a_pinuse=$(printf '%s' "$log" | sed -n 's/.*vstat\[after\]: pages alloc [0-9]* freed [0-9]* inuse \([0-9]*\).*/\1/p' | head -1)
b_pinuse=$(printf '%s' "$log" | sed -n 's/.*vstat\[before\]: pages alloc [0-9]* freed [0-9]* inuse \([0-9]*\).*/\1/p' | head -1)

if [ -z "$b_vused" ] || [ -z "$a_vused" ]; then
    echo "  FAIL could not parse vstat counters" >&2; fails=$((fails + 1))
else
    echo "  info vnode used: $b_vused -> $a_vused"
    echo "  info pages: alloc $a_palloc freed $a_pfreed inuse $b_pinuse -> $a_pinuse"
    if [ "$a_vused" -le "$b_vused" ]; then
        echo "  ok   vnode pool returned to baseline"
    else
        echo "  FAIL vnode leak: $b_vused -> $a_vused" >&2; fails=$((fails + 1))
    fi
    if [ "$a_pinuse" -le $((b_pinuse + 8)) ]; then
        echo "  ok   physical pages returned to baseline"
    else
        echo "  FAIL page leak: $b_pinuse -> $a_pinuse" >&2; fails=$((fails + 1))
    fi
    if [ "${a_palloc:-0}" -gt 0 ] && [ "${a_pfreed:-0}" -gt 0 ]; then
        echo "  ok   pages were allocated and recycled"
    else
        echo "  FAIL no page recycling observed (alloc ${a_palloc:-?} freed ${a_pfreed:-?})" >&2
        fails=$((fails + 1))
    fi
fi

printf '%s' "$log" | grep -q 'TRAP' && { echo "  FAIL kernel trap" >&2; fails=$((fails+1)); }

if [ "$fails" -ne 0 ]; then echo "S10.1: FAILED ($fails check(s))" >&2; exit 1; fi
echo "S10.1: PASSED"
exit 0
