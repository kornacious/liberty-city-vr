param([Parameter(Mandatory=$true)][string]$StageRoot)
$ErrorActionPreference = 'Stop'
$source = Get-Content -Raw (Join-Path $StageRoot 'src/save/GenericGameStorage.cpp')
$header = Get-Content -Raw (Join-Path $StageRoot 'src/save/GenericGameStorage.h')
if ($header -notmatch '#define SLOT_COUNT \(8\)' -or $header -notmatch 'AUTOSAVE_SLOT = -1') {
    throw 'Keep eight manual slots and a distinct autosave load target.'
}
if ($source -notmatch 'AutosaveTransaction::Save\(operations, ValidSaveName\)') {
    throw 'The engine must use the tested autosave transaction.'
}
$functions = @('GetSaveFileName', 'CheckSaveFile', 'CheckDataNotCorrupt')
$extracted = foreach ($name in $functions) {
    $match = [regex]::Match($source, ('(?ms)^(?:static )?bool\s+' + $name + '\(.*?^}'))
    if (-not $match.Success) { throw "Cannot extract production function $name" }
    $match.Value
}
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'autosave-production.inc'), ($extracted -join "`n"))
$frontend = Get-Content -Raw (Join-Path $StageRoot 'src/core/Frontend.cpp')
if ($frontend -notmatch 'case MENUACTION_LOADAUTOSAVE:\s+m_nCurrSaveSlot = AUTOSAVE_SLOT;\s+ChangeScreen\(MENUPAGE_LOAD_SLOT_CONFIRM') {
    throw 'Autosave menu action must select the dedicated slot and load confirmation.'
}
if ($source -match 'PcSaveHelper.SaveSlot\(0\)') { throw 'Autosave must not overwrite manual slot 1.' }
foreach ($menu in @('MenuScreens.cpp', 'MenuScreensCustom.cpp')) {
    $text = Get-Content -Raw (Join-Path $StageRoot ('src/core/' + $menu))
    if ([regex]::Matches($text, 'MENUACTION_LOADAUTOSAVE').Count -ne 1) { throw "Missing dedicated load action: $menu" }
}
& (Join-Path $PSScriptRoot 'test-ragdoll.ps1') -KitRoot $StageRoot -Sources @('test-autosave.cpp')
