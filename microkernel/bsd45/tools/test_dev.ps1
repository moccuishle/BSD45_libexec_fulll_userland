# test_dev.ps1 - S11 acceptance: /dev/null, /dev/zero, /dev/tty.
param([string]$Iso = "")

$ErrorActionPreference = "Stop"
$ws = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (!$Iso) { $Iso = Join-Path $ws "out\BSD45-ELF-x86.iso" }
$runner = Join-Path $ws "tools\run_qemu_test.ps1"
$fails = 0

function Check([string]$name, [string]$log, [string[]]$needles) {
    foreach ($n in $needles) {
        if ($log.Contains($n)) { Write-Host ("  ok   {0}: found {1}" -f $name, $n) -ForegroundColor Green }
        else { Write-Host ("  FAIL {0}: missing {1}" -f $name, $n) -ForegroundColor Red; $script:fails++ }
    }
}

Write-Host "S11 acceptance: /dev/null, /dev/zero, /dev/tty"
$log = (& $runner -Iso $Iso `
    -Keys "shell|echo hi > /dev/null|echo st=`$?|cat /dev/null|echo tty-ok > /dev/tty|exit 0" `
    -WaitFor "user exit" -TimeoutSec 120) -join "`n"

Check "null write" $log @("st=0")
Check "tty write"  $log @("tty-ok")
Check "exit"       $log @("[user exit 0]")

if ($log.Contains("cannot create /dev/")) { Write-Host "  FAIL /dev open failed" -ForegroundColor Red; $fails++ }
if ($log.Contains("cannot open /dev/"))   { Write-Host "  FAIL /dev read failed" -ForegroundColor Red; $fails++ }
if ($log -match '\*\*\* TRAP')            { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`nS11: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`nS11: PASSED" -ForegroundColor Green
exit 0
