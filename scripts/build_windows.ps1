# Builds the Windows plugin (VST3 + standalone), runs the sound tests and makes the installer.
param([string]$BuildNumber = "0")
$ErrorActionPreference = "Stop"

$base = (Select-String -Path CMakeLists.txt -Pattern 'set\(MU_VERSION_BASE "([^"]+)"\)').Matches[0].Groups[1].Value
$version = "$base.$BuildNumber"

cmake -B build -G "Visual Studio 17 2022" -A x64 -DMU_BUILD_TESTS=ON -DMU_COPY_AFTER_BUILD=OFF "-DMU_BUILD_NUMBER=$BuildNumber"
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

cmake --build build --config Release --target MusicallyUnorthodox_VST3 MusicallyUnorthodox_Standalone MUTests --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$tests = Get-ChildItem -Recurse build -Filter MUTests.exe | Select-Object -First 1
& $tests.FullName
if ($LASTEXITCODE -ne 0) { throw "sound tests failed" }

$artefacts = Join-Path $PWD "build\MusicallyUnorthodox_artefacts\Release"
if (!(Test-Path (Join-Path $artefacts "VST3\Musically Unorthodox.vst3"))) { throw "the VST3 was not built where expected" }
if (!(Test-Path (Join-Path $artefacts "Standalone\Musically Unorthodox.exe"))) { throw "the standalone app was not built where expected" }

$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (!(Test-Path $iscc)) { choco install innosetup -y --no-progress; if ($LASTEXITCODE -ne 0) { throw "could not install Inno Setup" } }
New-Item -ItemType Directory -Force dist | Out-Null
& $iscc "/DAppVersion=$version" "/DArtefacts=$artefacts" "/O$(Join-Path $PWD 'dist')" scripts\windows_installer.iss
if ($LASTEXITCODE -ne 0) { throw "installer failed" }
Copy-Item dist\MusicallyUnorthodox-Windows-Setup.exe "dist\MusicallyUnorthodox-$version-build$BuildNumber-Windows-Setup.exe"
Get-ChildItem dist
