# vcpkg-install.ps1
# Run this once (per triplet) before calling cmake --preset.
# It pre-installs all manifest packages so cmake configure doesn't need to
# spawn vcpkg internally (which causes lock contention with the VS generator).
#
# Usage:
#   .\vcpkg-install.ps1 msvc-x64-static
#   .\vcpkg-install.ps1 msvc-x86-static
#   .\vcpkg-install.ps1 mingw-x64-static-release
#   .\vcpkg-install.ps1 mingw-x86-static-release

param(
    [Parameter(Mandatory)]
    [ValidateSet(
        "msvc-x64-static",
        "msvc-x86-static",
        "mingw-x64-static-release",
        "mingw-x64-static-debug",
        "mingw-x86-static-release",
        "mingw-x86-static-debug"
    )]
    [string]$Preset
)

$tripletMap = @{
    "msvc-x64-static"          = "x64-windows-static"
    "msvc-x86-static"          = "x86-windows-static"
    "mingw-x64-static-release" = "x64-mingw-static"
    "mingw-x64-static-debug"   = "x64-mingw-static"
    "mingw-x86-static-release" = "x86-mingw-static"
    "mingw-x86-static-debug"   = "x86-mingw-static"
}

$triplet = $tripletMap[$Preset]
$installRoot = "$PSScriptRoot\build\$Preset\vcpkg_installed"

# Locate vcpkg
$vcpkg = $null
if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    $vcpkgBundle = "$env:VCPKG_ROOT\vcpkg-bundle.json"
    $readonly = $false
    if (Test-Path $vcpkgBundle) {
        $bundle = Get-Content $vcpkgBundle -Raw | ConvertFrom-Json
        $readonly = $bundle.readonly -eq $true
    }
    if ($readonly) {
        Write-Warning "VCPKG_ROOT points to a read-only VS-bundled vcpkg. Searching for standalone..."
    } else {
        $vcpkg = "$env:VCPKG_ROOT\vcpkg.exe"
    }
}

if (-not $vcpkg) {
    $candidates = @("C:\vcpkg", "$HOME\vcpkg", "$HOME\source\vcpkg", "$HOME\source\repos\vcpkg")
    foreach ($c in $candidates) {
        if (Test-Path "$c\vcpkg.exe") {
            $vcpkg = "$c\vcpkg.exe"
            break
        }
    }
}

if (-not $vcpkg) {
    Write-Error "vcpkg not found. Install standalone vcpkg:`n  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg`n  C:\vcpkg\bootstrap-vcpkg.bat`nThen set `$env:VCPKG_ROOT = 'C:\vcpkg'"
    exit 1
}

Write-Host "Using vcpkg: $vcpkg"
Write-Host "Triplet:     $triplet"
Write-Host "Install dir: $installRoot"
Write-Host ""

New-Item -ItemType Directory -Force -Path $installRoot | Out-Null

& $vcpkg install `
    --triplet $triplet `
    --x-manifest-root "$PSScriptRoot" `
    --x-install-root "$installRoot" `
    --overlay-triplets "$PSScriptRoot\triplets"

if ($LASTEXITCODE -ne 0) {
    Write-Error "vcpkg install failed (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Done. Now run:"
Write-Host "  cmake --preset $Preset"
Write-Host "  cmake --build --preset $Preset-release   (or -debug)"
