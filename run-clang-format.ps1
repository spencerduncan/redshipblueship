Using Namespace System
$url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.6/LLVM-14.0.6-win64.exe"
$llvmInstallerPath = ".\LLVM-14.0.6-win64.exe"
$clangFormatFilePath = ".\clang-format.exe"
$requiredVersion = "clang-format version 14.0.6"
$currentVersion = ""

function Test-7ZipInstalled {
    $sevenZipPath = "C:\Program Files\7-Zip\7z.exe"
    return Test-Path $sevenZipPath -PathType Leaf
}

if (Test-Path $clangFormatFilePath) {
    $currentVersion = & $clangFormatFilePath --version
    if (-not ($currentVersion -eq $requiredVersion)) {
        # Delete the existing file if the version is incorrect
        Remove-Item $clangFormatFilePath -Force
    }
}

if (-not (Test-Path $clangFormatFilePath) -or ($currentVersion -ne $requiredVersion)) {
    if (-not (Test-7ZipInstalled)) {
        Write-Host "7-Zip is not installed. Please install 7-Zip and run the script again."
        exit
    }

    $wc = New-Object net.webclient
    $wc.Downloadfile($url, $PSScriptRoot + $llvmInstallerPath)

    $sevenZipPath = "C:\Program Files\7-Zip\7z.exe"
    $specificFileInArchive = "bin\clang-format.exe"
    & "$sevenZipPath" e $llvmInstallerPath $specificFileInArchive

    Remove-Item $llvmInstallerPath -Force
}

# Format the RSBS-enforced subset. Mirrors run-clang-format.sh, which reads
# the same list — .github/clang-format-paths.txt is the single source of truth
# for both scripts so they cannot drift. See that file's header for why the
# gate is an incremental allowlist over the game trees (RSBS-owned trees such
# as src/common/ and rsbs/ are enrolled whole).
$basePath = (Resolve-Path .).Path
$pathsFile = "$basePath\.github\clang-format-paths.txt"

if (-not (Test-Path $pathsFile -PathType Leaf)) {
    Write-Host "error: $pathsFile is missing - the format gate has no path list and would be a no-op"
    exit 1
}

$entries = @(Get-Content $pathsFile `
    | Where-Object { $_ -notmatch '^\s*#' -and $_ -notmatch '^\s*$' })

# Guard against this gate silently becoming a no-op again (it targeted a
# nonexistent soh\ directory for the whole life of the repo): an empty list is
# an error, and so is any entry that no longer exists on disk. A renamed or
# deleted file must fail loudly rather than silently shrink coverage.
if ($entries.Length -eq 0) {
    Write-Host "error: $pathsFile lists no paths - the format gate would be a no-op"
    exit 1
}

# An entry ending in '/' enrols a whole RSBS-owned tree: every tracked
# .c/.cc/.cpp/.h/.hpp file under it (same rule as run-clang-format.sh). Any
# other entry is a single file.
$files = @()
$missing = $false
foreach ($e in $entries) {
    if ($e.EndsWith('/')) {
        if (-not (Test-Path (Join-Path $basePath ($e -replace '/', '\')) -PathType Container)) {
            Write-Host "error: $pathsFile lists '$e', which does not exist (renamed or deleted?)"
            $missing = $true
            continue
        }
        $tree = @(git ls-files -- $e | Where-Object { $_ -match '\.(c|cc|cpp|h|hpp)$' })
        if ($tree.Length -eq 0) {
            Write-Host "error: $pathsFile lists '$e', which holds no tracked C/C++ files"
            $missing = $true
            continue
        }
        $files += @($tree | ForEach-Object { Join-Path $basePath ($_ -replace '/', '\') })
    } else {
        $f = Join-Path $basePath ($e -replace '/', '\')
        if (-not (Test-Path $f -PathType Leaf)) {
            Write-Host "error: $pathsFile lists '$e', which does not exist (renamed or deleted?)"
            $missing = $true
        } else {
            $files += $f
        }
    }
}
if ($missing) {
    Write-Host "error: fix the stale entries above, or the format gate silently loses coverage"
    exit 1
}

# Format in batches: one clang-format process per file is very slow on Windows
$batchSize = 40
for ($i = 0; $i -lt $files.Length; $i += $batchSize) {
    $batch = $files[$i..([Math]::Min($i + $batchSize, $files.Length) - 1)]
    $last = $batch[-1].Substring($basePath.Length + 1)
    Write-Host "Formatting [$([Math]::Min($i + $batchSize, $files.Length))/$($files.Length)] ... $last"
    .\clang-format.exe -i $batch
}
