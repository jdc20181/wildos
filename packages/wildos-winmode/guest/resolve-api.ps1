# Resolves every type and member wild-guest.exe refers to against the .NET
# Framework on this Windows machine, so a new entry in api.txt is backed by
# real Windows rather than by mono. Run it with Windows PowerShell 5.1 (it runs
# on .NET Framework 4.8; pwsh runs on .NET and would prove nothing):
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File resolve-api.ps1 wild-guest.exe
#
# If it reports nothing missing, rewrite the list with
# check-api.py wild-guest.exe api.txt --write.
#
# References whose parent is an open generic (!0, !!0) cannot be resolved
# without the generic context they are used in, so they are skipped; their
# closed forms are resolved where the program instantiates them.
param([Parameter(Mandatory)][string]$Exe)

$module = [System.Reflection.Assembly]::LoadFile((Resolve-Path $Exe)).ManifestModule
$resolved = 0; $skipped = 0; $missing = 0

# TypeRef, MemberRef, TypeSpec, MethodSpec
foreach ($table in 0x01, 0x0A, 0x1B, 0x2B) {
    for ($row = 1; ; $row++) {
        $token = ($table -shl 24) -bor $row
        try {
            if ($table -eq 0x01 -or $table -eq 0x1B) { [void]$module.ResolveType($token) }
            else { [void]$module.ResolveMember($token) }
            $resolved++
        } catch {
            $e = $_.Exception
            while ($e.InnerException) { $e = $e.InnerException }
            if ($e -is [System.ArgumentOutOfRangeException] -or $e -is [System.ArgumentException]) { break }
            if ($e -is [System.BadImageFormatException]) { $skipped++; continue }
            Write-Output ("missing 0x{0:X8}: {1}" -f $token, $e.Message)
            $missing++
        }
    }
}

Write-Output "resolved $resolved, open generics skipped $skipped, missing $missing (.NET $([System.Runtime.InteropServices.RuntimeEnvironment]::GetSystemVersion()))"
if ($missing) { exit 1 }
