# bundle_windows.ps1 <BinaryPath> <OutDir>
#
# Copies the binary and all non-system DLL dependencies into OutDir using
# dumpbin (MSVC) or objdump (MinGW). Called from the Windows wheel CI job.

param(
    [Parameter(Mandatory)][string]$BinaryPath,
    [Parameter(Mandatory)][string]$OutDir
)

$ErrorActionPreference = "Stop"
$BinaryPath = Resolve-Path $BinaryPath
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# DLLs that come from Windows itself — never bundle these.
$SystemDlls = @(
    "kernel32.dll", "user32.dll", "gdi32.dll", "winspool.dll",
    "comdlg32.dll", "advapi32.dll", "shell32.dll", "ole32.dll",
    "oleaut32.dll", "uuid.dll", "odbc32.dll", "odbccp32.dll",
    "ntdll.dll", "msvcrt.dll", "vcruntime140.dll", "vcruntime140_1.dll",
    "msvcp140.dll", "api-ms-win-*.dll", "ext-ms-*.dll",
    "ws2_32.dll", "rpcrt4.dll", "winmm.dll", "avrt.dll",
    "bcrypt.dll", "crypt32.dll", "secur32.dll", "mfplat.dll",
    "mf.dll", "mfreadwrite.dll", "mfuuid.dll"
)

function Is-SystemDll([string]$name) {
    $lower = $name.ToLower()
    foreach ($pat in $SystemDlls) {
        if ($lower -like $pat) { return $true }
    }
    return $false
}

function Get-DllDeps([string]$path) {
    # Use dumpbin if available, else fall back to objdump (MSYS2/MinGW)
    if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
        dumpbin /dependents $path 2>$null |
            Select-String '^\s+\S+\.dll' |
            ForEach-Object { $_.ToString().Trim() }
    }
    elseif (Get-Command objdump -ErrorAction SilentlyContinue) {
        objdump -p $path 2>$null |
            Select-String 'DLL Name:' |
            ForEach-Object { ($_ -replace '.*DLL Name:\s*', '').Trim() }
    }
}

# Collect all DLL search directories from PATH
$SearchDirs = $env:PATH -split ";" | Where-Object { $_ -ne "" }

function Find-Dll([string]$name) {
    foreach ($dir in $SearchDirs) {
        $candidate = Join-Path $dir $name
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

$visited = @{}
$queue = [System.Collections.Queue]::new()
$queue.Enqueue($BinaryPath)

while ($queue.Count -gt 0) {
    $current = $queue.Dequeue()
    $deps = Get-DllDeps $current
    foreach ($dep in $deps) {
        if ([string]::IsNullOrWhiteSpace($dep)) { continue }
        if (Is-SystemDll $dep) { continue }
        if ($visited.ContainsKey($dep.ToLower())) { continue }
        $visited[$dep.ToLower()] = $true

        $found = Find-Dll $dep
        if ($found -and -not (Test-Path (Join-Path $OutDir $dep))) {
            Copy-Item $found (Join-Path $OutDir $dep)
            Write-Host "  bundled $dep"
            $queue.Enqueue($found)
        }
    }
}

# Copy the binary itself
$binName = Split-Path $BinaryPath -Leaf
Copy-Item $BinaryPath (Join-Path $OutDir $binName) -Force

$libCount = (Get-ChildItem $OutDir -Filter "*.dll").Count
Write-Host "Bundled $libCount DLLs + binary into $OutDir"
