# test_s10_3.ps1 - S10.3 acceptance: rm/cp/mv tools + rename(2) + EROFS.
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

Write-Host "S10.3 acceptance: rm / cp / mv / wc / grep + rename(2) + read-only ISO"
$log = (& $runner -Iso $Iso -Keys @(
    "shell",
    "cp /readme.txt /tmp/r",
    "cat /tmp/r",
    "wc /tmp/r",
    "grep BSD45 /tmp/r",
    "mv /tmp/r /tmp/r2",
    "ls /tmp",
    "rm /tmp/r2",
    "ls /tmp",
    "mkdir /tmp/x",
    "rm -r /tmp/x",
    "rm /readme.txt",
    "echo st=`$?",
    "exit 0") -WaitFor "user exit" -TimeoutSec 150) -join "`n"

Check "cp/cat"    $log @("BSD45-ELF-x86  -  4.4BSD-Lite2 i386 ELF kernel")
if ($log -match "\d+ \d+ \d+") { Write-Host "  ok   wc: printed <lines> <words> <bytes>" -ForegroundColor Green } else { Write-Host "  FAIL wc: no line/word/byte counts" -ForegroundColor Red; $fails++ }
Check "grep"      $log @("ET_EXEC")
Check "mv"        $log @("r2")
Check "rm"        $log @("Read-only file system")
Check "mkdir/rm"  $log @("st=1")
Check "exit"      $log @("[user exit 0]")
if ($log -match '\*\*\* TRAP') { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`nS10.3: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`nS10.3: PASSED" -ForegroundColor Green
exit 0
