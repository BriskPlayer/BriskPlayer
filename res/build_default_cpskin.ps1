<#
.SYNOPSIS
    Rebuilds res/Default.CPSkin, folding the classic main/EQ/shade window
    skins (as Skin.ini + bitmaps) into the same zip archive that already
    holds the playlist skin (Skin.def + playlist bitmaps).

.DESCRIPTION
    res/Default.CPSkin has no reproducible build step today - it's a
    hand-built zip checked into git. This script re-derives it from:
      - the archive's own current contents (Skin.def + 24 playlist bitmaps),
        carried through byte-for-byte
      - the 13 loose classic-skin PNGs already in res/
      - the 3 hand-authored Skin.ini files in res/skin_ini/
    Run this manually whenever a skin asset or Skin.ini changes; it is not
    part of the CMake build.
#>

param(
    [string]$SourceZip = (Join-Path $PSScriptRoot "Default.CPSkin"),
    [string]$OutputZip = (Join-Path $PSScriptRoot "Default.CPSkin"),
    [string]$SkinIniDir = (Join-Path $PSScriptRoot "skin_ini")
)

Add-Type -AssemblyName System.IO.Compression.FileSystem

$staging = Join-Path $env:TEMP "cpskin_build_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $staging | Out-Null

try {
    # 1. Extract today's existing entries verbatim (Skin.def + playlist bitmaps)
    [System.IO.Compression.ZipFile]::ExtractToDirectory($SourceZip, $staging)
    $existingCount = (Get-ChildItem -Recurse -File $staging).Count
    Write-Host "Carried forward $existingCount existing entries from $SourceZip"

    # 2. Stage the 13 loose classic-skin PNGs into their variant folders
    New-Item -ItemType Directory -Force -Path "$staging\Normal", "$staging\EQ", "$staging\Shade" | Out-Null

    $copies = [ordered]@{
        "normal_up.png"      = "Normal\MainUp.png"
        "eq_down.png"        = "Normal\MainDown.png"    # shared with EQ variant
        "eq_sw.png"          = "Normal\MainSwitch.png"  # shared with EQ variant
        "normal_numbers.png" = "Normal\BigFont.png"
        "normal_text.png"    = "Normal\SmallFont.png"

        "eq_up.png"          = "EQ\MainUp.png"
        "eq_numbers.png"     = "EQ\BigFont.png"
        "eq_text.png"        = "EQ\SmallFont.png"

        "shade_up.png"       = "Shade\MainUp.png"
        "shade_down.png"     = "Shade\MainDown.png"
        "shade_sw.png"       = "Shade\MainSwitch.png"
        "shade_time.png"     = "Shade\TimeFont.png"
        "shade_text.png"     = "Shade\TextFont.png"
    }
    foreach ($srcName in $copies.Keys) {
        $src = Join-Path $PSScriptRoot $srcName
        if (-not (Test-Path $src)) {
            throw "Missing source PNG: $src"
        }
        Copy-Item $src (Join-Path $staging $copies[$srcName])
    }
    Write-Host "Staged $($copies.Count) classic-skin bitmaps"

    # 3. Stage the 3 hand-authored Skin.ini files
    $inis = [ordered]@{
        "Normal_Skin.ini" = "Normal\Skin.ini"
        "EQ_Skin.ini"     = "EQ\Skin.ini"
        "Shade_Skin.ini"  = "Shade\Skin.ini"
    }
    foreach ($srcName in $inis.Keys) {
        $src = Join-Path $SkinIniDir $srcName
        if (-not (Test-Path $src)) {
            throw "Missing Skin.ini source: $src"
        }
        Copy-Item $src (Join-Path $staging $inis[$srcName])
    }
    Write-Host "Staged $($inis.Count) Skin.ini files"

    # 4. Re-zip. ZipFile.CreateFromDirectory stores entry names with the
    #    OS-native separator - on Windows that's '\', not '/' - matching
    #    what BmpCoolUp=Normal\MainUp.png etc. in the Skin.ini files
    #    reference literally, and what CF_FindFile's exact stricmp expects
    #    (it does no path normalization).
    $tempZip = Join-Path $env:TEMP "Default_new_$([guid]::NewGuid().ToString('N')).CPSkin"
    if (Test-Path $tempZip) { Remove-Item $tempZip -Force }
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $staging, $tempZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)

    # 5. Smoke-test: reopen and report entry count/names before replacing
    #    the committed file. This proves the file is a *valid* zip to .NET;
    #    it does NOT prove CompositeFile.c's stricter local-header-only
    #    parser will accept it - actually launching the app and cycling
    #    through all three skin variants is the real test (see the plan's
    #    verification section).
    $check = [System.IO.Compression.ZipFile]::OpenRead($tempZip)
    $entryCount = $check.Entries.Count
    Write-Host "Built archive with $entryCount entries:"
    $check.Entries | Sort-Object FullName | ForEach-Object { Write-Host "  $($_.FullName)" }
    $check.Dispose()

    $expectedCount = $existingCount + $copies.Count + $inis.Count
    if ($entryCount -ne $expectedCount) {
        throw "Entry count mismatch: expected $expectedCount, got $entryCount"
    }

    Copy-Item $tempZip $OutputZip -Force
    Write-Host "Wrote $OutputZip"
}
finally {
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
    if ($tempZip -and (Test-Path $tempZip)) { Remove-Item $tempZip -Force }
}
