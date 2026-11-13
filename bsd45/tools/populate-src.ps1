# populate-src.ps1 - copy the 4.5BSD sys tree files that the build needs
# into the kernel/ layout, WITHOUT overwriting any file the project ships.
#
# The repository only carries the files the project modified/added.  The
# remaining 4.5BSD sources come from src\BSD45 (unpacked from the tarball by
# tools\ensure-src.ps1).  This script does the "reorganize" copy in an
# idempotent, non-destructive way: existing (project) files always win.
param([string]$ws = "")

$ErrorActionPreference = "Stop"
if (!$ws) { $ws = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path) }

$src = Join-Path $ws "src\BSD45\usr\src\sys"
$k   = Join-Path $ws "kernel"
if (!(Test-Path $src)) { throw "4.5BSD sys tree missing: $src (run tools\ensure-src.ps1)" }

$copied = 0; $kept = 0
function Copy-Missing([string]$from, [string]$to) {
    if (!(Test-Path $from)) { return }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    Get-ChildItem $from -File -Force -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Name -in @("config.mk", "tags", ".keep")) { return }
        if ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) { return }
        $dst = Join-Path $to $_.Name
        if (Test-Path $dst) { $script:kept++; return }
        try {
            Copy-Item -LiteralPath $_.FullName -Destination $dst -Force -ErrorAction Stop
            $script:copied++
        } catch {
            Write-Host "  skip $($_.Name): $($_.Exception.Message.Split([char]10)[0])"
        }
    }
}

# ---- headers
Copy-Missing (Join-Path $src "sys")          (Join-Path $k "include\sys")
Copy-Missing (Join-Path $src "i386\include")  (Join-Path $k "include\machine")
if (Test-Path (Join-Path $k "include\machine")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $k "include\i386") | Out-Null
    Copy-Missing (Join-Path $k "include\machine") (Join-Path $k "include\i386")
}
Copy-Missing (Join-Path $src "vm")            (Join-Path $k "vm")

# i386/include/{ansi,limits,float}.h are symlinks to ../../hp300/include in
# the 4.5BSD tarball; Windows extraction turns them into broken links, so
# take the real files from hp300/include.
foreach ($h in @("ansi.h", "limits.h", "float.h")) {
    $dst = Join-Path $k "include\machine\$h"
    $hp  = Join-Path $src "hp300\include\$h"
    $ok = (Test-Path $dst) -and ((Get-Item $dst -ErrorAction SilentlyContinue).Length -gt 0)
    if (!$ok -and (Test-Path $hp)) {
        try {
            Copy-Item -LiteralPath $hp -Destination $dst -Force -ErrorAction Stop
            Write-Host "  $h <- hp300/include (symlink workaround)"
        } catch { Write-Host "  could not install $h" }
    }
}

# ---- libkern
Copy-Missing (Join-Path $src "libkern")       (Join-Path $k "lib")
if (Test-Path (Join-Path $k "lib\libkern.h")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $k "include\lib") | Out-Null
    Copy-Item (Join-Path $k "lib\libkern.h") (Join-Path $k "include\lib\libkern.h") -Force
}

# ---- i386 machine dependent
Copy-Missing (Join-Path $src "i386\i386")     (Join-Path $k "arch\i386\i386")
Copy-Missing (Join-Path $src "i386\isa")      (Join-Path $k "arch\i386\isa")

# ---- kern + fs + net + dev
Copy-Missing (Join-Path $src "kern")          (Join-Path $k "sys")
foreach ($d in @("ufs", "miscfs", "isofs", "nfs")) {
    Copy-Missing (Join-Path $src $d) (Join-Path $k "fs\$d")
}
Copy-Missing (Join-Path $src "dev")           (Join-Path $k "driver\dev")
foreach ($d in @("net", "netinet", "netiso")) {
    Copy-Missing (Join-Path $src $d) (Join-Path $k "net\$d")
}

Write-Host "populate-src: copied $copied file(s), kept $kept project file(s)"
