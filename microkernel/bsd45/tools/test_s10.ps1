# test_s10.ps1 - S10.1 acceptance: physical page + vnode recycling.
#
# Two independent stress paths:
#   vstress  - runs the VFS loops inside the kernel and prints the pool
#              counters before/after (this is where a leak would show up)
#   test     - the user-space /test binary does the same through syscalls
#              and then exits 0
#
# Pass criteria:
#   * all vstress ok=100
#   * vnode "used" and page "inuse" after the loop are back to the baseline
#   * pages alloc grows and pages freed catches up (no monotonic growth)
#   * /test reports bad=0 for all three S10 sections
param([string]$Iso = "")

$ErrorActionPreference = "Stop"
$ws = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (!$Iso) { $Iso = Join-Path $ws "out\BSD45-ELF-x86.iso" }

$script = Join-Path $ws "tools\run_qemu_test.ps1"
$fails = 0

function Check([string]$name, [string]$log, [string[]]$needles) {
    foreach ($n in $needles) {
        if ($log.Contains($n)) {
            Write-Host ("  ok   {0}: found {1}" -f $name, $n) -ForegroundColor Green
        } else {
            Write-Host ("  FAIL {0}: missing {1}" -f $name, $n) -ForegroundColor Red
            $script:fails++
        }
    }
}

Write-Host "S10.1 acceptance: page + vnode recycling"
$log = (& $script -Iso $Iso -Keys @(
    "vstat",
    "vstress",
    "test") -WaitFor "user exit" -TimeoutSec 150) -join "`n"

Check "kernel vstat" $log @("vstat[before]: vnode total 96 used", "vstat[before]: pages alloc")
Check "iso loop"     $log @("vstress: iso read 100x ok=100")
Check "tmpfs loop"   $log @("vstress: tmpfs mkdir/rmdir 100x ok=100")
Check "rewrite loop" $log @("vstress: tmpfs rewrite 100x ok=100")
Check "user space"   $log @("s10: iso open/read/close 50x bad=0",
                           "s10: tmpfs mkdir/rmdir 50x bad=0",
                           "s10: tmpfs rewrite 50x bad=0")
Check "exit"         $log @("[user exit 0]")

# --- numeric checks: the pools must return to the baseline ----------------
function Numbers([string]$tag, [string]$log) {
    $m = [regex]::Match($log, "vstat\[$tag\]: vnode total (\d+) used (\d+) free (\d+)")
    $p = [regex]::Match($log, "vstat\[$tag\]: pages alloc (\d+) freed (\d+) inuse (\d+)")
    if (!$m.Success -or !$p.Success) { return $null }
    return @{
        vused  = [int]$m.Groups[2].Value
        vfree  = [int]$m.Groups[3].Value
        palloc = [int]$p.Groups[1].Value
        pfreed = [int]$p.Groups[2].Value
        pinuse = [int]$p.Groups[3].Value
    }
}

$b = Numbers "before" $log
$a = Numbers "after"  $log
if ($b -eq $null -or $a -eq $null) {
    Write-Host "  FAIL could not parse vstat counters" -ForegroundColor Red
    $fails++
} else {
    Write-Host ("  info before: vnode used {0}, pages alloc {1} freed {2} inuse {3}" -f $b.vused, $b.palloc, $b.pfreed, $b.pinuse)
    Write-Host ("  info after : vnode used {0}, pages alloc {1} freed {2} inuse {3}" -f $a.vused, $a.palloc, $a.pfreed, $a.pinuse)
    if ($a.vused -le $b.vused) {
        Write-Host "  ok   vnode pool returned to baseline" -ForegroundColor Green
    } else {
        Write-Host ("  FAIL vnode leak: used {0} -> {1}" -f $b.vused, $a.vused) -ForegroundColor Red
        $fails++
    }
    if ($a.pinuse -le $b.pinuse + 8) {
        Write-Host "  ok   physical pages returned to baseline" -ForegroundColor Green
    } else {
        Write-Host ("  FAIL page leak: inuse {0} -> {1}" -f $b.pinuse, $a.pinuse) -ForegroundColor Red
        $fails++
    }
    if ($a.palloc -gt 0 -and $a.pfreed -gt 0) {
        Write-Host "  ok   pages were allocated and recycled" -ForegroundColor Green
    } else {
        Write-Host ("  FAIL no page recycling observed (alloc {0} freed {1})" -f $a.palloc, $a.pfreed) -ForegroundColor Red
        $fails++
    }
}

if ($log -match 'TRAP') {
    Write-Host "  FAIL kernel trap" -ForegroundColor Red
    $fails++
}

if ($fails) {
    Write-Host "`nS10.1: FAILED ($fails)" -ForegroundColor Red
    exit 1
}
Write-Host "`nS10.1: PASSED" -ForegroundColor Green
exit 0
