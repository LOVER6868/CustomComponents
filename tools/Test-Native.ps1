param(
    [Parameter(Mandatory=$true)][string]$SprigRoot,
    [string]$NdkRoot=$env:ANDROID_NDK_ROOT,
    [string]$VcVars
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$SprigRoot=(Resolve-Path -LiteralPath $SprigRoot).Path
$build=Join-Path $root '.build/tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$compiler=Join-Path $NdkRoot 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe'
if(!(Test-Path -LiteralPath $compiler)) { throw 'Set ANDROID_NDK_ROOT or pass -NdkRoot.' }
$flags=@('--target=aarch64-linux-android21','-std=c++20','-Wno-invalid-offsetof','-fsyntax-only',
    '-DBLOOM_MODULE_NAME="CustomComponents"','-DBLOOM_MODULE_TYPE=::Bloom::Type::PnP',
    "-I$root/inc","-I$SprigRoot/mod/include","-I$SprigRoot/deps/BNM-Android/include",
    "-I$SprigRoot/deps/BNM-Android/external/include","-I$SprigRoot/deps/Dobby/include")
foreach($mode in @('default','rollback')) {
    $extra=@()
    if($mode -eq 'rollback') { $extra=@('-DCUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE=0','-DCUSTOMCOMPONENTS_PF3_EXACT_ROUTING=0','-DCUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING=0') }
    & $compiler @flags @extra "$root/main.cpp"
    if($LASTEXITCODE -ne 0) { throw "ARM64 syntax failed: $mode" }
    Write-Host "PASS ARM64 syntax: $mode"
}
if(!$VcVars) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if(Test-Path -LiteralPath $vswhere) {
        $installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if($installation) { $VcVars=Join-Path $installation 'VC/Auxiliary/Build/vcvars64.bat' }
    }
}
if(!$VcVars -or !(Test-Path -LiteralPath $VcVars)) { throw 'Pass -VcVars pointing to Visual Studio vcvars64.bat for host tests.' }
$cases=@('ReturnToLanePolicyRegression','ReturnToLaneMoveFilterRegression','Patch3SubmissionLifetimeRegression','SelectionTransaction216Regression',
    'PhysicalDeckSelection222Regression','Pick3VerifiedBindingRegression')
$commands=@('@echo off',"call `"$VcVars`" >nul",'if errorlevel 1 exit /b 1')
foreach($case in $cases) {
    $support=''
    if($case -eq 'Pick3VerifiedBindingRegression') { $support=" `"$root/tests/host/log_stub.cpp`"" }
    $commands+="cl /nologo /std:c++20 /EHsc /I`"$root/inc`" /I`"$root/tests/host`" /I`"$SprigRoot/mod/include`" `"$root/tests/$case.cpp`"$support /Fe:`"$build/$case.exe`" /Fo:`"$build/`""
    $commands+='if errorlevel 1 exit /b 1'
    $commands+="`"$build/$case.exe`""
    $commands+='if errorlevel 1 exit /b 1'
    $commands+="echo PASS $case"
}
$script=Join-Path $build 'run-host.cmd'
$commands | Set-Content -LiteralPath $script -Encoding ascii
& cmd.exe /d /c "`"$script`""
if($LASTEXITCODE -ne 0) { throw 'Host regression failure.' }
