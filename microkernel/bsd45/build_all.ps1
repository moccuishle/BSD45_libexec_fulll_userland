# build_all.ps1 - build userland + sh + /test + kernel + ISO in one shot
param(
    [switch]$NoSh      # keep the small built-in sh instead of the 4.5BSD one
)

$ErrorActionPreference = "Stop"
$ws = Split-Path -Parent $MyInvocation.MyCommand.Path

function Invoke-Step([string]$script, [string[]]$extra = @()) {
    $args = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $ws $script)) + $extra
    & powershell @args
    if ($LASTEXITCODE -ne 0) { throw "$script failed" }
}

Invoke-Step "build_userland.ps1"

if (-not $NoSh) {
    # S3: the real 4.5BSD /bin/sh (overwrites out\userland\sh.elf), plus the
    # small tools (/bin/echo, /bin/ls, /bin/cat, /bin/mkdir).
    Invoke-Step "build_sh.ps1"
    if (!(Test-Path (Join-Path $ws "out\userland\sh.elf"))) { throw "sh build failed" }

    # /test: the process/tty/tmpfs/proc self test that the monitor runs.
    Invoke-Step "build_sh.ps1" @("-Target", "test")
}

Invoke-Step "build_kernel.ps1"
Invoke-Step "build_iso.ps1"

Write-Host ""
Write-Host "Artifacts:"
Get-ChildItem (Join-Path $ws "out") | Select-Object Name, Length

exit 0
