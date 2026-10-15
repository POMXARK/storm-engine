# Builds the engine on Windows in one step, from a plain PowerShell or a double click on build.bat:
# - enters the x64 developer shell of the newest Visual Studio (2022 or 2019, Build Tools too): the
#   compiler and the Windows SDK are in PATH only there;
# - puts the Scripts folders of Python in PATH (pip puts conan.exe there and often does not add it);
# - installs by pip what is missing: Conan 2.12.2, Ninja >= 1.10 (the preset builds with Ninja Multi-Config), CMake >= 3.24
#   (Visual Studio 2019 brings 3.20), Pillow (the generators of the demos);
# - makes the Conan profile and runs the preset default; the engine is build/default/<Config>/engine.exe.
# Usage: build.ps1 [-Config Debug|Release] [-ExtraModules <folder of an engine module of a game>]
param([ValidateSet('Debug', 'Release')][string]$Config = 'Debug', [string]$ExtraModules = '')
$ErrorActionPreference = 'Stop'

function Assert-Exit([string]$What) { if ($LASTEXITCODE -ne 0) { throw "$What failed: $LASTEXITCODE" } }

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw 'no Visual Studio: install Build Tools for Visual Studio 2022 with "Desktop development with C++"' }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio has no C++ compiler: add "Desktop development with C++" in Visual Studio Installer' }
    Write-Host "Visual Studio: $vs"
    Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

$python = $null
foreach ($candidate in @(@('py', '-3'), @('python'), @('python3')))
{
    if (Get-Command $candidate[0] -ErrorAction SilentlyContinue) { $python = $candidate; break }
}
if ($null -eq $python) { throw 'no Python 3: install it from python.org' }
$pyExe = $python[0]
$pyArgs = @($python | Select-Object -Skip 1)

# before the developer shell's own tools: an old CMake of Visual Studio must not win. The code goes by
# stdin: Windows PowerShell 5.1 drops the quotes inside the arguments of a program
$scripts = "import os, sysconfig`nprint(sysconfig.get_path('scripts'))`nprint(sysconfig.get_path('scripts', os.name + '_user'))" | & $pyExe @pyArgs -
Assert-Exit 'python'
foreach ($dir in $scripts) { $env:PATH = "$dir;$env:PATH" }

# true when the program is in PATH and its version is at least Major.Minor
function Test-Version([string]$Program, [int]$Major, [int]$Minor)
{
    if (-not (Get-Command $Program -ErrorAction SilentlyContinue)) { return $false }
    $line = (& $Program --version | Select-Object -First 1)
    if ($line -notmatch '(\d+)\.(\d+)') { return $false }
    return ([int]$Matches[1] -gt $Major) -or ([int]$Matches[1] -eq $Major -and [int]$Matches[2] -ge $Minor)
}
$missing = @()
if (-not (Get-Command conan -ErrorAction SilentlyContinue)) { $missing += 'conan==2.12.2' }
# Ninja Multi-Config needs ninja 1.10 (include in its files); Visual Studio 2019 brings an older one
if (-not (Test-Version ninja 1 10)) { $missing += 'ninja>=1.10' }
if (-not (Test-Version cmake 3 24)) { $missing += 'cmake>=3.24' }
# Windows PowerShell 5.1 turns redirected stderr of a program into an error, which Stop would throw
$ErrorActionPreference = 'Continue'
& $pyExe @pyArgs -c 'import PIL' 2>$null
if ($LASTEXITCODE -ne 0) { $missing += 'Pillow' }
$ErrorActionPreference = 'Stop'
if ($missing.Count -gt 0)
{
    Write-Host "pip install --user $missing"
    & $pyExe @pyArgs -m pip install --user @missing
    Assert-Exit 'pip install'
}
Write-Host "cmake: $((Get-Command cmake).Source)"
Write-Host "ninja: $((Get-Command ninja).Source) $(& ninja --version)"

# conan profile path fails while there is no profile; cmd keeps its error text off the PowerShell errors
cmd /c 'conan profile path default >nul 2>&1'
if ($LASTEXITCODE -ne 0) { & conan profile detect; Assert-Exit 'conan profile detect' }

$cmakeArgs = @('--preset=default')
if ($ExtraModules) { $cmakeArgs += "-DSTORM_EXTRA_MODULES=$((Resolve-Path $ExtraModules).Path)" }
Push-Location $PSScriptRoot
try
{
    # a configure that failed without a compiler leaves a cache that the next one trips on
    if ((Test-Path 'build/default/CMakeCache.txt') -and -not (Test-Path 'build/default/build.ninja'))
    {
        Remove-Item -Recurse -Force 'build/default'
    }
    & cmake @cmakeArgs
    Assert-Exit 'cmake configure'
    & cmake --build --preset=$($Config.ToLower())
    Assert-Exit 'cmake build'
}
finally { Pop-Location }
Write-Host "engine: $(Join-Path $PSScriptRoot "build\default\$Config\engine.exe")"
