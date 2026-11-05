#requires -Version 5.1
<#
.SYNOPSIS
Removes generated files and archives legacy root notes from a CustomComponents project.
.EXAMPLE
.\Clean-Project.ps1 -WhatIf
.EXAMPLE
.\Clean-Project.ps1
#>
[CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='Medium')]
param([string]$ProjectRoot=$PSScriptRoot)

$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $ProjectRoot).ProviderPath.TrimEnd('\','/')
foreach($required in @('main.cpp','bloom.ini','config.ini','inc')) {
    if(!(Test-Path -LiteralPath (Join-Path $root $required))) {
        throw "Not a CustomComponents project root: $root (missing $required)"
    }
}
$prefix=$root+[IO.Path]::DirectorySeparatorChar
function Assert-LocalPath([string]$Path) {
    $absolute=[IO.Path]::GetFullPath($Path)
    if(!$absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup target is outside the project: $absolute"
    }
    # The selected project root is the trust boundary. Its ancestors may be
    # OneDrive folders or junctions; only redirects INSIDE the project matter.
    $current=$absolute
    while($current -and !$current.Equals($root,[StringComparison]::OrdinalIgnoreCase)) {
        if(Test-Path -LiteralPath $current) {
            $item=Get-Item -LiteralPath $current -Force
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Cleanup will not traverse a link or junction: $current"
            }
        }
        $parent=Split-Path -Path $current -Parent
        if($parent -eq $current) { break }
        $current=$parent
    }
    return $absolute
}
function Get-GeneratedFiles([string]$Directory) {
    foreach($item in Get-ChildItem -LiteralPath $Directory -Force) {
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if($item.PSIsContainer) {
            if($item.Name -in @('.git','.build','__pycache__')) { continue }
            Get-GeneratedFiles $item.FullName
        } elseif($item.Name -in @('.DS_Store','Thumbs.db') -or $item.Extension -in @('.pyc','.pyo')) {
            $item
        }
    }
}
function Get-CacheDirectories([string]$Directory) {
    foreach($item in Get-ChildItem -LiteralPath $Directory -Force -Directory) {
        if($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if($item.Name -in @('.git','.build')) { continue }
        if($item.Name -eq '__pycache__') { $item }
        else { Get-CacheDirectories $item.FullName }
    }
}

# Extracting a release over an older module leaves files that the new ZIP no
# longer contains. Archive only known old report names; keep current/unknown docs.
$legacy=@(Get-ChildItem -LiteralPath $root -Force -File | Where-Object {
    $_.Name -match '^(PATCH[0-9_].*\.md|PF3_.*\.md|PickFromThree_Trigger_Queue_Roadmap_.*\.md|CHANGESET_.*\.json|REMOVE_OBSOLETE_1727\.ps1)$'
})
foreach($file in $legacy) {
    $source=Assert-LocalPath $file.FullName
    $name=$file.Name
    if($file.Extension -eq '.ps1') { $name+='.txt' }
    $destination=Assert-LocalPath (Join-Path $root ('docs/history/imported-root/'+$name))
    if(Test-Path -LiteralPath $destination) {
        $sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $destinationHash=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if($sourceHash -eq $destinationHash) {
            if($PSCmdlet.ShouldProcess($source,'Remove identical already-archived report')) {
                Remove-Item -LiteralPath $source -Force
            }
            continue
        }
        # Keep differing files instead of replacing existing historical notes.
        $version=1
        $base=[IO.Path]::GetFileNameWithoutExtension($name)
        $extension=[IO.Path]::GetExtension($name)
        do {
            $destination=Assert-LocalPath (Join-Path $root ("docs/history/imported-root/$base-$version$extension"))
            $version++
        } while(Test-Path -LiteralPath $destination)
    }
    if($PSCmdlet.ShouldProcess($source,"Archive legacy report to $destination")) {
        New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
        Move-Item -LiteralPath $source -Destination $destination
        Write-Host "Archived: $($file.Name)"
    }
}

$targets=@()
$build=Join-Path $root '.build'
if(Test-Path -LiteralPath $build) { $targets+=Get-Item -LiteralPath $build -Force }
$targets+=@(Get-CacheDirectories $root)
$targets+=@(Get-GeneratedFiles $root)
# Exact legacy outputs only. Never treat arbitrary executables, logs, ZIPs,
# or asset binaries as disposable merely because of their extension.
foreach($name in @('patch3-test.exe','patch3-test.obj','transaction-test.exe',
    'transaction-test.obj','run-host-tests.cmd','syntax.log','stock-placement.asm')) {
    $candidate=Join-Path $root $name
    if(Test-Path -LiteralPath $candidate -PathType Leaf) {
        $targets+=Get-Item -LiteralPath $candidate -Force
    }
}
foreach($target in $targets) {
    $path=Assert-LocalPath $target.FullName
    if($target.PSIsContainer) {
        # Reject a linked descendant before any recursive removal begins.
        $pending=New-Object 'System.Collections.Generic.Stack[string]'
        $pending.Push($path)
        while($pending.Count) {
            foreach($child in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
                $null=Assert-LocalPath $child.FullName
                if($child.PSIsContainer) { $pending.Push($child.FullName) }
            }
        }
    }
    if($PSCmdlet.ShouldProcess($path,'Remove generated project output')) {
        Remove-Item -LiteralPath $path -Recurse -Force
        Write-Host "Removed: $path"
    }
}
Write-Host 'Cleanup complete. Source, configuration, assets, documentation, Git data, and release archives are preserved.'
