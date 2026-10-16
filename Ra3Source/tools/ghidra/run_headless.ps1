<#
.SYNOPSIS
    Runs Ghidra headless against the persistent Ra3Source analysis project.

.DESCRIPTION
    -Import creates the project (if needed), imports the game executable named
    by config/ra3source.ini and its SkuDef (set-exe), and runs auto-analysis.
    Otherwise the given post-script runs on the already analysed program,
    read-only unless -Save is passed.

    The game installation is only read. The project lives in <repo>/work/ghidra.

.EXAMPLE
    .\run_headless.ps1 -Import
    .\run_headless.ps1 -Script ExportProgramOverview.java -ScriptArgs D:\out
#>
[CmdletBinding(DefaultParameterSetName = 'Script')]
param(
    [Parameter(ParameterSetName = 'Import', Mandatory)] [switch] $Import,
    [Parameter(ParameterSetName = 'Script', Mandatory)] [string] $Script,
    [Parameter(ParameterSetName = 'Script')] [string[]] $ScriptArgs = @(),
    [Parameter(ParameterSetName = 'Script')] [switch] $Save,
    [string] $ProjectDir = (Join-Path $PSScriptRoot '..\..\..\work\ghidra'),
    [string] $ProjectName = 'RA3',
    [string] $MaxMem = '16G'
)

$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Read-Ini([string] $Path) {
    $values = @{}
    $section = ''
    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $t = $line.Trim()
        if ($t -eq '' -or $t.StartsWith(';') -or $t.StartsWith('#')) { continue }
        if ($t -match '^\[(.+)\]$') { $section = $Matches[1].Trim().ToLower(); continue }
        if ($t -match '^([^=]+)=(.*)$') { $values["$section.$($Matches[1].Trim().ToLower())"] = $Matches[2].Trim().Trim('"') }
    }
    $values
}

function Resolve-GameExecutable {
    $configFile = if ($env:RA3_CONFIG) { $env:RA3_CONFIG } else { Join-Path $sourceRoot 'config\ra3source.ini' }
    $ini = Read-Ini $configFile
    $local = [IO.Path]::ChangeExtension($configFile, '.local.ini')
    if (Test-Path -LiteralPath $local) { (Read-Ini $local).GetEnumerator() | ForEach-Object { $ini[$_.Key] = $_.Value } }
    $root = if ($env:RA3_GAME_ROOT) { $env:RA3_GAME_ROOT } else { $ini['game.root'] }
    if (-not [IO.Path]::IsPathRooted($root)) { $root = Join-Path (Split-Path $configFile) $root }
    $skuDef = Join-Path $root $ini['game.skudef']
    $exeLine = Get-Content -LiteralPath $skuDef | Where-Object { $_ -match '^\s*set-exe\s+(.+)$' } | Select-Object -First 1
    if (-not $exeLine) { throw "no set-exe line in $skuDef" }
    $null = $exeLine -match '^\s*set-exe\s+(.+)$'
    Join-Path $root $Matches[1].Trim()
}

if (-not $env:GHIDRA_INSTALL_DIR) { throw 'GHIDRA_INSTALL_DIR is not set' }
if (-not $env:JAVA_HOME) { throw 'JAVA_HOME is not set' }
$analyzeHeadless = Join-Path $env:GHIDRA_INSTALL_DIR 'support\analyzeHeadless.bat'
$env:GHIDRA_HEADLESS_MAXMEM = $MaxMem

$ProjectDir = [IO.Path]::GetFullPath($ProjectDir)
$null = New-Item -ItemType Directory -Force $ProjectDir
$logDir = [IO.Path]::GetFullPath((Join-Path $ProjectDir '..\logs'))
$null = New-Item -ItemType Directory -Force $logDir
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

if ($Import) {
    $binary = Resolve-GameExecutable
    Write-Host "importing $binary"
    & $analyzeHeadless $ProjectDir $ProjectName -import $binary -log (Join-Path $logDir "ghidra-import-$stamp.log")
} else {
    $program = Split-Path -Leaf (Resolve-GameExecutable)
    [string[]] $mode = @()
    if (-not $Save) { $mode += '-readOnly' }
    & $analyzeHeadless $ProjectDir $ProjectName -process $program -noanalysis @mode `
        -scriptPath $PSScriptRoot -postScript $Script @ScriptArgs `
        -log (Join-Path $logDir "ghidra-script-$stamp.log") -scriptlog (Join-Path $logDir "ghidra-script-$stamp.script.log")
}
exit $LASTEXITCODE
