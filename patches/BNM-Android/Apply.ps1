[CmdletBinding(SupportsShouldProcess=$true)]
param([Parameter(Mandatory=$true)][string]$SprigRoot)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($SprigRoot).TrimEnd('\','/')
$relative='deps/BNM-Android/src/Exceptions.cpp'
$target=[IO.Path]::GetFullPath((Join-Path $root $relative))
if(!$target.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Dependency path escaped Sprig root.'}
# A user-selected root may have OneDrive ancestors. Reject redirects inside it.
$current=$target
while(!$current.Equals($root,[StringComparison]::OrdinalIgnoreCase)) {
    if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Dependency redirect: $current"}
    $current=Split-Path $current -Parent
}
$source=Join-Path $PSScriptRoot 'src/Exceptions.cpp'
$expected=(Get-FileHash -LiteralPath $source).Hash
$actual=(Get-FileHash -LiteralPath $target).Hash
if($actual -eq $expected){Write-Host 'BNM TryInvoke fix is already installed.';return}
if($actual -ne 'FCF604E86C354582FF86D0916ADCF5E4DABAD962EEF503CCFE93C666D200B325') {
    throw 'BNM Exceptions.cpp differs from the audited version. Review it before applying this patch.'
}
if($PSCmdlet.ShouldProcess($target,'Initialize all TryInvoke metadata')) {
    Copy-Item -LiteralPath $source -Destination $target -Force
    if((Get-FileHash -LiteralPath $target).Hash -ne $expected){throw 'Dependency patch verification failed.'}
    Write-Host 'Installed and verified BNM TryInvoke fix. Rebuild libsprig.so and the APK.'
}
