<#
.SYNOPSIS
  Remove obsolete PickFromThree documentation and reference-art clutter.
.DESCRIPTION
  Run in Bloom\CustomComponents or via absolute path. Deletion only, no backup
  directories, no renamed files. Native static embed source PNGs are protected.
.EXAMPLE
  .\Clean-Pick3Junk.ps1 -WhatIf
  .\Clean-Pick3Junk.ps1
#>
[CmdletBinding(SupportsShouldProcess=$true,ConfirmImpact='Low')]
param()
$ErrorActionPreference='Stop'
$Root=$PSScriptRoot
if (!(Test-Path -LiteralPath (Join-Path $Root 'main.cpp') -PathType Leaf) -or
    !(Test-Path -LiteralPath (Join-Path $Root 'inc\presentation\PickFromThreePreviewSystem.hpp') -PathType Leaf) -or
    !(Test-Path -LiteralPath (Join-Path $Root 'docs\PICKFROMTHREE.md') -PathType Leaf)) {
    throw 'Expected the current CustomComponents folder and docs\PICKFROMTHREE.md.'
}
# Keep specific, verified runtime/source locations unconditionally.
$obsolete = @(
    'PICK3_ART_TRACE_TEST.txt',
    'PICK3_HOLDER_INSPECT_ADB_TEST.txt',
    'FACTORY_PROBE_ADB_TEST.txt',
    'METADATA_SOURCE_ADB_TEST.txt',
    'METADATA_SNAPSHOT_TEST.txt',
    'PICK3_REMOVED_FILES.txt',
    'STAGE8_TEST_PLAN.txt',
    'docs\NEXT_PICK3_PATCH.md',
    'assets\pick3-static\README.md',
    'examples\pick-from-three\README.md',
    'inc\presentation\detail\Pick3SamplePortraitBytes.hpp',
    'src\Pick3SamplePortraitBytes.cpp',
    'assets\pick3-static\selected_button.png',
    'assets\pick3-static\portrait_disc.png',
    'assets\pick3-static\portrait_rim.png',
    'assets\pick3-static\class_ring.png'
)
$removed=0
foreach($rel in $obsolete){
    $item=Join-Path $Root $rel
    if(Test-Path -LiteralPath $item -PathType Leaf){
        if($PSCmdlet.ShouldProcess($item,'Delete obsolete Pick3 file')){
            Remove-Item -LiteralPath $item -Force
            $removed++
        }
    }
}
$docs=Join-Path $Root 'docs'
# Older dated PICK3_*.md have been rewritten into docs\PICKFROMTHREE.md.
foreach($file in @(Get-ChildItem -LiteralPath $docs -Filter 'PICK3_*.md' -File)){
    if($PSCmdlet.ShouldProcess($file.FullName,'Delete consolidated Pick3 note')){
        Remove-Item -LiteralPath $file.FullName -Force
        $removed++
    }
}
foreach($dirName in @('docs\native-static-art')){
    $dir=Join-Path $Root $dirName
    if(Test-Path -LiteralPath $dir -PathType Container){
        if($PSCmdlet.ShouldProcess($dir,'Delete redundant Pick3 reference images')){
            Remove-Item -LiteralPath $dir -Recurse -Force
            $removed++
        }
    }
}
Write-Host "PickFromThree cleanup finished: $removed obsolete entries deleted."
Write-Host 'Retained: .git, actual C++ code, all regeneration PNG inputs in assets\pick3-static, config.ini.'
Write-Host 'No backup folder was created.'
