# test_mount.ps1 - S12 acceptance: /dev/mounts + /bin/df.
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

Write-Host "S12 acceptance: /dev/mounts + /bin/df"
$log = (& $runner -Iso $Iso -Keys "shell|df|mount|cat /dev/mounts|exit 0" `
    -WaitFor "user exit" -TimeoutSec 120) -join "`n"

Check "df header" $log @("Filesystem  Mounted on  Size")
Check "mount"     $log @("device on mount point type")
# The ISO root LBA depends on the mastering tool (VMware mkisofs: 24,
# MSYS2 xorriso: 19), so only assert the shape of the line, not the LBA.
Check "iso mount" $log @("iso9660 lba=")
Check "tmpfs"     $log @("tmpfs")
Check "rw/ro"     $log @(" ro", " rw")
Check "exit"      $log @("[user exit 0]")

if ($log.Contains("cannot open")) { Write-Host "  FAIL /dev/mounts unreadable" -ForegroundColor Red; $fails++ }
if ($log -match '\*\*\* TRAP')    { Write-Host "  FAIL kernel trap" -ForegroundColor Red; $fails++ }

if ($fails) { Write-Host "`nS12: FAILED ($fails)" -ForegroundColor Red; exit 1 }
Write-Host "`nS12: PASSED" -ForegroundColor Green
exit 0
