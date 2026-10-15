# The steps of run.sh for Windows, shared by the run.ps1 of the demos. A demo dot-sources it:
#   $Demo = $PSScriptRoot; . "$Demo/../common/demo.ps1" @args
# The first argument is the engine; when it is left out or is an option (--editor), the Debug build
# of the default preset is taken: build/default/Debug/engine.exe. A game sets $DefaultEngine before.
$ErrorActionPreference = 'Stop'

if (-not $DefaultEngine) { $DefaultEngine = Join-Path $Demo '../../build/default/Debug/engine.exe' }
$DefaultEngine = [System.IO.Path]::GetFullPath($DefaultEngine)
$Engine = $DefaultEngine
$EngineArgs = @($args)
if ($EngineArgs.Count -gt 0 -and -not ([string]$EngineArgs[0]).StartsWith('-'))
{
    $Engine = $EngineArgs[0]
    $EngineArgs = @($EngineArgs | Select-Object -Skip 1)
}
if (-not (Test-Path $Engine -PathType Leaf)) { throw "no engine at ${Engine}: build it (build.bat) or pass its path" }
$Engine = (Resolve-Path $Engine).Path

# python.org installs the launcher py and python, the Microsoft Store one python3 too
$Python = $null
foreach ($candidate in @(@('py', '-3'), @('python'), @('python3')))
{
    if (Get-Command $candidate[0] -ErrorAction SilentlyContinue) { $Python = $candidate; break }
}
if ($null -eq $Python) { throw 'no Python 3: install it from python.org, then pip install Pillow numpy' }

# runs a generator of the assets; a failed one stops the demo, as set -e in run.sh
function Invoke-Python
{
    $exe = $Python[0]
    # not -I: the isolated mode skips the user site, where pip install --user puts numpy and Pillow
    $options = @($Python | Select-Object -Skip 1) + $args
    & $exe @options
    if ($LASTEXITCODE -ne 0) { throw "python $($args[0]) failed: $LASTEXITCODE" }
}

# the shaders are copied next to the engine by the build: a junction needs neither admin rights nor
# developer mode, unlike the symbolic link of run.sh; a copy is the last resort
function Set-Techniques
{
    $link = Join-Path $Demo 'resource/techniques'
    $target = Join-Path (Split-Path $Engine) 'resource/techniques'
    if (Test-Path $link)
    {
        $item = Get-Item $link -Force
        if ($item.LinkType) { $item.Delete() } else { Remove-Item -Recurse -Force $link }
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $link) | Out-Null
    # off Windows a junction is not made and no error is raised: a symbolic link is tried then
    foreach ($type in 'Junction', 'SymbolicLink')
    {
        if (-not (Test-Path $link)) { try { New-Item -ItemType $type -Path $link -Target $target | Out-Null } catch { } }
    }
    if (-not (Test-Path $link)) { Copy-Item -Recurse -Path $target -Destination $link }
}

# the engine looks for program/ and resource/ from its working folder: it runs in the folder of the demo
function Start-Demo([string[]]$Folders = @())
{
    Set-Techniques
    foreach ($folder in $Folders) { New-Item -ItemType Directory -Force -Path (Join-Path $Demo $folder) | Out-Null }
    # engine.exe is a GUI program on Windows: PowerShell does not wait for one started by &, so the exit
    # code was lost and run.bat returned at once
    $start = @{ FilePath = $Engine; WorkingDirectory = $Demo; Wait = $true; PassThru = $true; NoNewWindow = $true }
    if ($EngineArgs.Count -gt 0) { $start.ArgumentList = $EngineArgs }
    $process = Start-Process @start
    exit $process.ExitCode
}
