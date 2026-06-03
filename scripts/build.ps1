param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Clean,
    [switch]$SingleFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $repoRoot 'build'
$distDir = Join-Path $repoRoot 'dist\ManageSoftCpp'
$distServerDir = Join-Path $repoRoot 'dist\ManageSoftServer'

$cmake = Join-Path $repoRoot '.venv\Lib\site-packages\cmake\data\bin\cmake.exe'
if (-not (Test-Path $cmake)) {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
}

$qtRoot = 'D:/Qt/5.15.2/mingw81_64'
$qtBinDir = Join-Path $qtRoot 'bin'
$cCompiler = 'D:/Qt/Tools/mingw810_64/bin/gcc.exe'
$cxxCompiler = 'D:/Qt/Tools/mingw810_64/bin/g++.exe'
$makeProgram = 'D:/App_Data/MINgw-64/mingw64/bin/mingw32-make.exe'
$toolchainBinDir = Split-Path $cxxCompiler -Parent

function Resolve-FirstExistingPath {
    param(
        [string[]]$CandidatePaths
    )

    foreach ($candidatePath in $CandidatePaths) {
        if (-not [string]::IsNullOrWhiteSpace($candidatePath) -and (Test-Path $candidatePath)) {
            return $candidatePath
        }
    }

    return $null
}

function New-SingleFileFrontendPackage {
    param(
        [string]$BundleDir,
        [string]$OutputExePath
    )

    $iexpress = Join-Path $env:WINDIR 'System32\iexpress.exe'
    if (-not (Test-Path $iexpress)) {
        Write-Warning 'IExpress not found. Skipping single-file frontend packaging.'
        return $null
    }

    $packageRoot = Join-Path $buildDir 'single-file-package'
    if (Test-Path $packageRoot) {
        Remove-Item $packageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $packageRoot | Out-Null

    $archivePath = Join-Path $packageRoot 'ManageSoftCppBundle.zip'
    $launcherScriptPath = Join-Path $packageRoot 'expand_and_launch.ps1'
    $launcherCmdPath = Join-Path $packageRoot 'launch_managesoftcpp.cmd'
    $sedPath = Join-Path $packageRoot 'manage_soft_cpp_single_file.sed'

    Compress-Archive -Path (Join-Path $BundleDir '*') -DestinationPath $archivePath -Force

    @'
param(
    [string]$ArchivePath = (Join-Path $PSScriptRoot 'ManageSoftCppBundle.zip')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$runtimeRoot = Join-Path $env:LOCALAPPDATA 'ManageSoftCppRuntime'
$extractRoot = Join-Path $runtimeRoot ([DateTime]::Now.ToString('yyyyMMddHHmmss'))

New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null
Expand-Archive -Path $ArchivePath -DestinationPath $extractRoot -Force

$entries = Get-ChildItem -Path $runtimeRoot -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
if ($entries.Count -gt 3) {
    $entries | Select-Object -Skip 3 | ForEach-Object {
        try {
            Remove-Item $_.FullName -Recurse -Force -ErrorAction Stop
        } catch {
        }
    }
}

Start-Process -FilePath (Join-Path $extractRoot 'ManageSoftCpp.exe')
'@ | Set-Content -Path $launcherScriptPath -Encoding ASCII

    @'
@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0expand_and_launch.ps1"
exit /b %ERRORLEVEL%
'@ | Set-Content -Path $launcherCmdPath -Encoding ASCII

    $sedContent = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$OutputExePath
FriendlyName=ManageSoftCpp
AppLaunched=cmd /c launch_managesoftcpp.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=$packageRoot
[SourceFiles0]
%FILE0%=
%FILE1%=
%FILE2%=
[Strings]
FILE0=ManageSoftCppBundle.zip
FILE1=expand_and_launch.ps1
FILE2=launch_managesoftcpp.cmd
"@
    Set-Content -Path $sedPath -Value $sedContent -Encoding ASCII

    if (Test-Path $OutputExePath) {
        Remove-Item $OutputExePath -Force
    }

    Write-Host '==> Packaging single-file frontend'
    & $iexpress /N $sedPath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'IExpress packaging failed.'
    }

    if (-not (Test-Path $OutputExePath)) {
        throw "Single-file package not found: $OutputExePath"
    }

    return $OutputExePath
}

$requiredPaths = @(
    @{ Label = 'CMake'; Path = $cmake },
    @{ Label = 'Qt Root'; Path = $qtRoot },
    @{ Label = 'C Compiler'; Path = $cCompiler },
    @{ Label = 'C++ Compiler'; Path = $cxxCompiler },
    @{ Label = 'Make Program'; Path = $makeProgram }
)

foreach ($entry in $requiredPaths) {
    if (-not (Test-Path $entry.Path)) {
        throw "$($entry.Label) not found: $($entry.Path)"
    }
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item $buildDir -Recurse -Force
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$configureArgs = @(
    '-S', $repoRoot,
    '-B', $buildDir,
    '-G', 'MinGW Makefiles',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_PREFIX_PATH=$qtRoot",
    "-DCMAKE_C_COMPILER=$cCompiler",
    "-DCMAKE_CXX_COMPILER=$cxxCompiler",
    "-DCMAKE_MAKE_PROGRAM=$makeProgram"
)

Write-Host "==> Configuring project ($Configuration)"
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw 'CMake configure failed.'
}

$parallelJobs = if ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { 4 }

Write-Host "==> Building project"
& $cmake --build $buildDir -j $parallelJobs
if ($LASTEXITCODE -ne 0) {
    throw 'CMake build failed.'
}

$builtExe = Join-Path $buildDir 'src\client\ManageSoftCpp.exe'
if (-not (Test-Path $builtExe)) {
    throw "Executable not found: $builtExe"
}

$builtServerExe = Join-Path $buildDir 'src\server\ManageSoftServer.exe'
if (-not (Test-Path $builtServerExe)) {
    throw "Backend executable not found: $builtServerExe"
}

if (Test-Path $distDir) {
    Remove-Item $distDir -Recurse -Force
}
New-Item -ItemType Directory -Path $distDir | Out-Null

if (Test-Path $distServerDir) {
    Remove-Item $distServerDir -Recurse -Force
}
New-Item -ItemType Directory -Path $distServerDir | Out-Null

$distExe = Join-Path $distDir 'ManageSoftCpp.exe'
Copy-Item $builtExe $distExe -Force

$distServerExe = Join-Path $distServerDir 'ManageSoftServer.exe'
Copy-Item $builtServerExe $distServerExe -Force

Write-Host "==> Deploying Qt runtime"
$runtimeFiles = @(
    (Join-Path $qtBinDir 'Qt5Core.dll'),
    (Join-Path $qtBinDir 'Qt5Gui.dll'),
    (Join-Path $qtBinDir 'Qt5Network.dll'),
    (Join-Path $qtBinDir 'Qt5Widgets.dll'),
    (Join-Path $qtBinDir 'Qt5Svg.dll'),
    (Join-Path $qtBinDir 'D3Dcompiler_47.dll'),
    (Join-Path $qtBinDir 'libEGL.dll'),
    (Join-Path $qtBinDir 'libGLESv2.dll'),
    (Join-Path $qtBinDir 'opengl32sw.dll'),
    (Join-Path $toolchainBinDir 'libgcc_s_seh-1.dll'),
    (Join-Path $toolchainBinDir 'libstdc++-6.dll'),
    (Join-Path $toolchainBinDir 'libwinpthread-1.dll')
)

$sslRuntimeFiles = @(
    $(Resolve-FirstExistingPath -CandidatePaths @(
        'C:/Python313/Lib/site-packages/PyQt5/Qt5/bin/libssl-1_1-x64.dll',
        'C:/Program Files/SteelSeries/GG/cvgamesense/_internal/libssl-1_1.dll',
        'C:/Python313/DLLs/libssl-3.dll',
        'C:/Program Files/Microsoft OneDrive/26.078.0426.0002/libssl-3-x64.dll'
    )),
    $(Resolve-FirstExistingPath -CandidatePaths @(
        'C:/Python313/Lib/site-packages/PyQt5/Qt5/bin/libcrypto-1_1-x64.dll',
        'C:/Program Files/SteelSeries/GG/cvgamesense/_internal/libcrypto-1_1.dll',
        'C:/Python313/DLLs/libcrypto-3.dll',
        'C:/Program Files/Microsoft OneDrive/26.078.0426.0002/libcrypto-3-x64.dll'
    ))
)

foreach ($sslRuntimeFile in $sslRuntimeFiles) {
    if (-not [string]::IsNullOrWhiteSpace($sslRuntimeFile)) {
        $runtimeFiles += $sslRuntimeFile
    }
}

foreach ($runtimeFile in $runtimeFiles) {
    if (Test-Path $runtimeFile) {
        Copy-Item $runtimeFile $distDir -Force
        Copy-Item $runtimeFile $distServerDir -Force
    }
}

$pluginDirs = @('platforms', 'styles', 'imageformats', 'iconengines', 'translations')
foreach ($pluginDirName in $pluginDirs) {
    $sourceDir = Join-Path $qtRoot "plugins\$pluginDirName"
    if ($pluginDirName -eq 'translations') {
        $sourceDir = Join-Path $qtRoot $pluginDirName
    }

    if (Test-Path $sourceDir) {
        $targetDir = Join-Path $distDir $pluginDirName
        Copy-Item $sourceDir $targetDir -Recurse -Force

        $serverTargetDir = Join-Path $distServerDir $pluginDirName
        Copy-Item $sourceDir $serverTargetDir -Recurse -Force
    }
}

$singleFileExe = $null
if ($SingleFile) {
    $singleFileExe = New-SingleFileFrontendPackage -BundleDir $distDir -OutputExePath (Join-Path $repoRoot 'dist\ManageSoftCpp-single.exe')
}

Write-Host ''
Write-Host 'Build completed successfully.'
Write-Host "Build directory : $buildDir"
Write-Host "Executable      : $builtExe"
Write-Host "Runnable bundle : $distDir"
Write-Host "Backend bundle  : $distServerDir"
if ($singleFileExe) {
    Write-Host "Single-file exe : $singleFileExe"
}