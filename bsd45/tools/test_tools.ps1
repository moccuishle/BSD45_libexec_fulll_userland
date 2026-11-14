# test_tools.ps1 - /bin/head, /bin/tail, /bin/uname, /bin/true, /bin/false.
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

Write-Host "tools acceptance: head/tail/uname/true/false"
$log = (& $runner -Iso $Iso `
    -Keys "shell|head -2 /readme.txt|tail -2 /readme.txt|head -1 /readme.txt|uname|uname -a|true && echo true-ok|false || echo false-ok|date|exit 0" `
    -WaitFor "user exit" -TimeoutSec 120) -join "`n"

Check "head"  $log @("BSD45-ELF-x86  -  4.4BSD-Lite2 i386 ELF kernel")
Check "head2" $log @("==============================================")
Check "tail"  $log @("bsd45> help")
Check "uname" $log @("BSD45 bsd45 1.0 4.4BSD-Lite2 i386 ELF kernel i386")
Check "true"  $log @("true-ok")
Check "false" $log @("false-ok")
if ($log -match '(Sun|Mon|Tue|Wed|Thu|Fri|Sat) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) +\d+ \d\d:\d\d:\d\d UTC \d{4}') {
    Write-Host "  ok   date: well-formed timestamp" -ForegroundColor Green
} else {
    Write-Host "  FAIL date: no well-formed timestamp" -ForegroundColor Red
    $fails++
}
Check "exit"  $log @("[user exit 0]")

if ($log -match '\*\*\* TRAP') { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }
if ($log.Contains("cannot open")) { Write-Host "  FAIL head/tail cannot open the file" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`ntools: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`ntools: PASSED" -ForegroundColor Green
exit 0
