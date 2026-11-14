# test_sig.ps1 - S10.2 acceptance: signal delivery + console job control.
#
# Covered:
#   * /test: handler runs (sigreturn), SIG_IGN, block/unblock,
#     default SIGTERM action reported as WIFSIGNALED/WTERMSIG
#   * shell: Ctrl-C on /bin/cat kills it with SIGINT, $? is 130
#   * shell: background job ("sleep &") shows up in `jobs`
#   * /bin/kill exists and lists signal names (`kill -l`)
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

Write-Host "S10.2 acceptance: signals + console job control"

# ---- 1) kernel/user signal semantics through /test ----------------------
$log = (& $script -Iso $Iso -Keys @("test") -WaitFor "user exit" -TimeoutSec 150) -join "`n"
Check "handler"   $log @("sig: handler ran", "sig: handler count=1")
Check "ignore"    $log @("sig: ignore ok")
Check "mask"      $log @("sig: blocked count=0", "sig: unblocked count=1")
Check "default"   $log @("sig: term signaled=1 sig=15")
Check "sig+exit"  $log @("sig: ignore-child exited=1 code=7", "[user exit 0]")

# ---- 2) Ctrl-C in the shell -> cat dies, $? == 130 ----------------------
$ctrlc = [string][char]3
$log = (& $script -Iso $Iso -Keys @("shell", "/bin/cat", $ctrlc, "echo st=`$?", "exit 0") `
    -WaitFor "user exit" -TimeoutSec 90) -join "`n"
Check "ctrlc"     $log @("st=130", "[user exit 0]")

# ---- 3) background job visible in jobs + /bin/kill present --------------
$log = (& $script -Iso $Iso -Keys @("shell", "sleep 5 &", "jobs", "kill -l", "exit 0") `
    -WaitFor "user exit" -TimeoutSec 90) -join "`n"
Check "jobs"      $log @("sleep 5")
Check "kill -l"   $log @("TERM 15", "KILL 9")

if ($log -match '\*\*\* TRAP') { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) {
    Write-Host "`nS10.2: FAILED ($fails)" -ForegroundColor Red
    exit 1
}
Write-Host "`nS10.2: PASSED" -ForegroundColor Green
exit 0
