# test_ps.ps1 - S13 acceptance: /dev/ps + /bin/ps.
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

Write-Host "S13 acceptance: /dev/ps + /bin/ps"
$log = (& $runner -Iso $Iso -Keys "shell|ps|cat /dev/ps|exit 0" `
    -WaitFor "user exit" -TimeoutSec 120) -join "`n"

Check "header"  $log @("PID PPID PGRP STAT COMM")
Check "shell"   $log @("1 0 1 BLOCK sh")
Check "self"    $log @("2 1 2 RUN ps")
Check "exit"    $log @("[user exit 0]")

if ($log.Contains("cannot open /dev/ps")) { Write-Host "  FAIL /dev/ps unreadable" -ForegroundColor Red; $fails++ }
if ($log -match '\*\*\* TRAP')            { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`nS13: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`nS13: PASSED" -ForegroundColor Green
exit 0
