# Windows version of fetch_assets.sh: downloads the free assets used by the dialog demo (licenses in
# CREDITS.md) unless they are there. Usage: fetch_assets.ps1 <assets dir>
param([Parameter(Mandatory)][string]$Dir)
$ErrorActionPreference = 'Stop'
$fonts = 'https://github.com/google/fonts/raw/main/ofl/ptserif'
New-Item -ItemType Directory -Force -Path $Dir | Out-Null
foreach ($file in 'PT_Serif-Web-Regular.ttf', 'PT_Serif-Web-Bold.ttf', 'OFL.txt')
{
    $path = Join-Path $Dir $file
    if (-not (Test-Path $path)) { Invoke-WebRequest -UseBasicParsing -Uri "$fonts/$file" -OutFile $path }
}
