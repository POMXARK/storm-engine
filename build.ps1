# Builds the engine on Windows in one step, from a plain PowerShell or a double click on build.bat:
# - enters the x64 developer shell of the newest Visual Studio (2022 or 2019, Build Tools too): the
#   compiler and the Windows SDK are in PATH only there;
# - puts the Scripts folders of Python in PATH (pip puts conan.exe there and often does not add it);
# - installs by pip what is missing: Conan 2.12.2, Ninja >= 1.11 and CMake 3.31 (Visual Studio 2019
#   brings ninja 1.10 and CMake 3.20), Pillow and numpy (the generators of the demos and of the game);
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

# English messages of cl: a localized /showIncludes prefix (non-ASCII on a Russian MSVC) goes into
# msvc_deps_prefix of rules.ninja, and the try_compile of the compiler check fails on a Russian Windows
# with "loading 'CMakeFiles\rules.ninja': The system cannot find the file specified"
$env:VSLANG = '1033'

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
# the try_compile of Ninja Multi-Config failed on Windows with CMake 3.25 and the ninja 1.10.2 of
# Visual Studio 2019 (loading 'CMakeFiles\rules.ninja'): the versions the engine is built with are taken
if (-not (Test-Version ninja 1 11)) { $missing += 'ninja>=1.11' }
if (-not (Test-Version cmake 3 28)) { $missing += 'cmake==3.31.10' }
# Windows PowerShell 5.1 turns redirected stderr of a program into an error, which Stop would throw
$ErrorActionPreference = 'Continue'
# the modules the generators of the assets import
foreach ($module in @(@('PIL', 'Pillow'), @('numpy', 'numpy')))
{
    & $pyExe @pyArgs -c "import $($module[0])" 2>$null
    if ($LASTEXITCODE -ne 0) { $missing += $module[1] }
}
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

$defines = @()
if ($ExtraModules) { $defines += "-DSTORM_EXTRA_MODULES=$((Resolve-Path $ExtraModules).Path)" }
$build = 'build/default'
$cache = "$build/CMakeCache.txt"

# the generator of the Visual Studio of the developer shell: VisualStudioVersion is 16.0 or 17.0
function Get-VsGenerator
{
    switch -Wildcard ($env:VisualStudioVersion)
    {
        '16.*' { return 'Visual Studio 16 2019' }
        '17.*' { return 'Visual Studio 17 2022' }
        default { throw "unknown Visual Studio version '$($env:VisualStudioVersion)'" }
    }
}

function Show-ConfigureLog
{
    # the reason of a failed compiler check is in the log, not on the screen
    foreach ($log in "$build/CMakeFiles/CMakeError.log", "$build/CMakeFiles/CMakeConfigureLog.yaml")
    {
        if (Test-Path $log) { Write-Host "--- $log"; Get-Content $log -Tail 40 | Write-Host }
    }
}

Push-Location $PSScriptRoot
try
{
    $generator = ''
    if (Test-Path $cache)
    {
        $line = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1
        if ($line) { $generator = $line.Matches[0].Groups[1].Value }
    }
    # a configure that failed without a compiler leaves a cache that the next one trips on
    if ((Test-Path $cache) -and -not (Test-Path "$build/build.ninja") -and -not (Test-Path "$build/*.sln"))
    {
        Remove-Item -Recurse -Force $build
        $generator = ''
    }

    if ($generator -like 'Visual Studio*') { & cmake -S . -B $build @defines }
    else
    {
        & cmake --preset=default @defines
        # on a Russian Windows the compiler check of Ninja fails: CMake writes no rules.ninja for the
        # localized /showIncludes of cl, and VSLANG does not help without the English language pack.
        # The generator of Visual Studio does not use them
        $log = "$build/CMakeFiles/CMakeConfigureLog.yaml"
        if ($LASTEXITCODE -ne 0 -and (Test-Path $log) -and (Select-String -Path $log -Pattern 'rules\.ninja' -Quiet))
        {
            $generator = Get-VsGenerator
            Write-Host "Ninja cannot check the compiler here; configuring with $generator"
            Remove-Item -Recurse -Force $build
            & cmake -S . -B $build -G $generator -A x64 @defines
        }
    }
    if ($LASTEXITCODE -ne 0) { Show-ConfigureLog }
    Assert-Exit 'cmake configure'
    & cmake --build $build --config $Config --parallel
    Assert-Exit 'cmake build'
}
finally { Pop-Location }
Write-Host "engine: $(Join-Path $PSScriptRoot "build\default\$Config\engine.exe")"
