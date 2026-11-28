# One check round for the woods range on the PC: build, optionally re-bake the ground and re-make materials,
# run the game's check pictures (holdover.RangeShots 1, windowed 1920x1080, no sound), make a contact sheet.
# Usage: powershell -File woods_round.ps1 -Name r2 [-Build] [-Bake] [-Materials]
param([string]$Name = "round", [switch]$Build, [switch]$Bake, [switch]$Materials)
$ErrorActionPreference = "Continue"
$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Proj = Join-Path $Repo "Holdover\Holdover.uproject"
$Eng = "C:\Program Files\Epic Games\UE_5.8\Engine"
$Log = Join-Path $Repo "Holdover\Saved\Logs\Holdover.log"
$Shots = Join-Path $Repo "Holdover\Saved\Screenshots\range"
$Out = Join-Path (Split-Path -Parent $Repo) "art\range\woods\$Name"

if (Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { Write-Output "ABORT: an Unreal process is running"; exit 1 }
if ($Build) {
    $r = & "$Eng\Build\BatchFiles\Build.bat" HoldoverEditor Win64 Development -Project="$Proj" -WaitMutex -NoHotReloadFromIDE 2>&1
    $r | Select-String -Pattern "error|Result:" | ForEach-Object { $_.Line }
    if (-not ($r -match "Result: Succeeded")) { Write-Output "ABORT: build failed"; exit 1 }
}
function Run-Script($s) {
    & "$Eng\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -run=pythonscript -script="$($Repo -replace '\\','/')/tools/unreal/$s.py" -unattended -nop4 -nosplash -NullRHI | Out-Null
    Select-String -Path $Log -Pattern "\[woods_|Error|Traceback" | Select-Object -Last 4 | ForEach-Object { $_.Line }
}
if ($Materials) { Run-Script "make_woods_materials" }
if ($Bake) { Run-Script "build_woods_ground" }
if (Test-Path $Shots) { Rename-Item $Shots ("range_prev_" + (Get-Date -Format "HHmmss")) }
$t0 = Get-Date
$p = Start-Process -FilePath "$Eng\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$Proj`" -game -windowed -ResX=1920 -ResY=1080 -nosound -ExecCmds=`"holdover.RangeShots 1`"" -PassThru
if (-not $p.WaitForExit(2400000)) { Write-Output "game still running after 40 min, killing"; $p.Kill() }
Write-Output ("game run {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
Select-String -Path $Log -Pattern "\[Holdover\] (Woods|Range shot)|Fatal" | ForEach-Object { $_.Line -replace '^.*\[Holdover\] ', '' } | Select-Object -Last 24
Select-String -Path $Log -Pattern "Failed to compile Material|missing usage flag" | ForEach-Object { ($_.Line -replace '^.*LogMaterial: Warning: ', '') } | Select-Object -Unique -First 12
New-Item -ItemType Directory -Force $Out | Out-Null
Copy-Item "$Shots\*.png" $Out
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "contact_sheet.ps1") $Out (Join-Path $Out "_sheet.jpg") 4 480
Write-Output "pictures: $Out"
