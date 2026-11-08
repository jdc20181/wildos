# Builds the Wild OS ISO into .\out using the builder container.
# Requires Docker Desktop with the WSL 2 backend.
# .\build-iso.ps1 --incremental reuses the last build (see build\incremental.sh).
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

docker build -t wildos-builder build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# out may be a junction to another drive (ISOs are 2-3 GB each). Docker cannot
# follow junctions in bind mounts, so mount the real folder.
$out = Join-Path $PSScriptRoot 'out'
if (-not (Test-Path $out)) { New-Item -ItemType Directory $out | Out-Null }
$outItem = Get-Item $out
if ($outItem.LinkType -eq 'Junction') { $out = $outItem.Target | Select-Object -First 1 }

docker run --rm --privileged `
    -v "${PSScriptRoot}:/src:ro" `
    -v "${out}:/out" `
    -v wildos-work:/work `
    -v wildos-cache:/cache `
    wildos-builder bash /src/build/build.sh @args
exit $LASTEXITCODE
