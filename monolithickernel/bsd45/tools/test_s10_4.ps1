# test_s10_4.ps1 - S10.4 acceptance: mount table + cwd + relative paths.
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

Write-Host "S10.4 acceptance: mount table, chdir/getcwd, relative paths"
$log = (& $runner -Iso $Iso `
    -Keys "shell|cd /tmp|pwd|echo vfs-rel > a|cat a|cd ..|pwd|cd /bin|ls|exit 0" `
    -WaitFor "user exit" -TimeoutSec 150) -join "`n"

Check "cd+pwd"    $log @("/tmp")
Check "rel write" $log @("vfs-rel")
Check "rel read"  $log @("vfs-rel")
Check "mount tmp" $log @("vfs-rel")
Check "cd /bin"   $log @("sh.elf")
Check "exit"      $log @("[user exit 0]")

if ($log.Contains("cannot create")) { Write-Host "  FAIL relative redirect still fails" -ForegroundColor Red; $fails++ }
if ($log -match '\*\*\* TRAP')      { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`nS10.4: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`nS10.4: PASSED" -ForegroundColor Green
exit 0
