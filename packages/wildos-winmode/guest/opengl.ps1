# Wild OS Windows mode: OpenGL for programs that need more than 1.1.
#
# Windows here has no graphics driver of its own (the virtual PC has a plain
# display adapter, and Remote Desktop draws the windows), so the only OpenGL
# is Microsoft's software one, which stops at version 1.1. A program that
# asks for anything newer gets no functions back and usually crashes at once
# (MagicaVoxel does, with an access violation at address 0).
#
# This puts Mesa's llvmpipe, a modern software OpenGL renderer, behind Windows'
# own opengl32.dll, the way mesa-dist-win's system-wide deployment does: the
# driver goes in as mesadrv.dll and is registered as the MSOGL driver, the
# one opengl32.dll uses when no graphics driver offers OpenGL. GALLIUM_DRIVER
# keeps Mesa on llvmpipe; left to choose, it takes its Direct3D 12 driver,
# which runs on Windows' WARP renderer and crashed MagicaVoxel too.
#
# Run by setup.ps1, and by the host for a Windows that was set up before
# this existed. It does nothing when the pinned release is already in place.

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$version = '26.2.5'
$url = "https://github.com/pal1000/mesa-dist-win/releases/download/$version/mesa3d-$version-release-msvc.7z"
$sha256 = '997123EF4CE8896204AB912D5B54F19372C487FF9835BCEFAFA2FCA23B8953C2'
$state = 'C:\ProgramData\WildOS'
$marker = "$state\opengl.json"

New-Item -ItemType Directory -Force -Path $state | Out-Null
$targets = @{ "$env:windir\System32\mesadrv.dll" = 'x64/libgallium_wgl.dll'; "$env:windir\SysWOW64\mesadrv.dll" = 'x86/libgallium_wgl.dll' }

$have = $null
if (Test-Path $marker) { $have = (Get-Content -Raw $marker | ConvertFrom-Json).version }
$present = @($targets.Keys | Where-Object { Test-Path $_ }).Count -eq $targets.Count
if ($have -eq $version -and $present) { 'already'; exit 0 }

$work = Join-Path $env:TEMP "wildos-opengl"
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    $archive = "$work\mesa.7z"
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive
    $got = (Get-FileHash -Algorithm SHA256 $archive).Hash
    if ($got -ne $sha256) { throw "the Mesa download does not match (got $got)" }
    # Windows' own tar.exe (libarchive) reads 7z.
    & "$env:windir\System32\tar.exe" -x -f $archive -C $work @($targets.Values)
    if ($LASTEXITCODE -ne 0) { throw "tar.exe could not unpack the Mesa download ($LASTEXITCODE)" }

    foreach ($target in $targets.Keys) {
        $source = Join-Path $work ($targets[$target] -replace '/', '\')
        # A program using the old copy keeps it open; a loaded DLL can be
        # renamed but not overwritten. Windows deletes the old copy at its
        # next start.
        if (Test-Path $target) {
            try { Remove-Item -Force $target }
            catch {
                $old = $target + (Get-Date -Format '.yyyyMMddHHmmss') + '.old'
                Rename-Item -Force $target (Split-Path -Leaf $old)
                if (-not ('WildOS.Native' -as [type])) { Add-Type -Namespace WildOS -Name Native -MemberDefinition '[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern bool MoveFileEx(string from, string to, int flags);' }
                # MOVEFILE_DELAY_UNTIL_REBOOT with no destination: delete then.
                [WildOS.Native]::MoveFileEx($old, [NullString]::Value, 4) | Out-Null
            }
        }
        Copy-Item -Force $source $target
    }

    foreach ($key in 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\MSOGL',
                     'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers\MSOGL') {
        New-Item -Force -Path $key | Out-Null
        New-ItemProperty -Force -Path $key -Name 'DLL' -Value 'mesadrv.dll' -PropertyType String | Out-Null
        New-ItemProperty -Force -Path $key -Name 'DriverVersion' -Value 1 -PropertyType DWord | Out-Null
        New-ItemProperty -Force -Path $key -Name 'Flags' -Value 1 -PropertyType DWord | Out-Null
        New-ItemProperty -Force -Path $key -Name 'Version' -Value 2 -PropertyType DWord | Out-Null
    }
    [Environment]::SetEnvironmentVariable('GALLIUM_DRIVER', 'llvmpipe', 'Machine')

    @{ version = $version; installed = (Get-Date).ToString('s') } | ConvertTo-Json -Compress | Set-Content -Encoding UTF8 $marker
    'installed'
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
