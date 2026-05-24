# Test build script for CSS Parser integration
# This script compiles and runs the CSS parser test

$ErrorActionPreference = "Stop"

Write-Host "Building CSS Parser Integration Test..." -ForegroundColor Cyan

# Paths
$projectRoot = "C:\Users\rehan\Documents\projects\Hyperion"
$engineInclude = "$projectRoot\engine\include"
$engineSrc = "$projectRoot\engine\src\css"
$engineLib = "$projectRoot\engine\lib"
$testFile = "$projectRoot\engine\tests\css_parser_test.cpp"
$buildDir = "$projectRoot\build\css_test"

# Create build directory
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

# Find cl.exe (MSVC compiler)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$clPath = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -find "VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe" 2>$null
if (-not $clPath) {
    Write-Host "Error: MSVC compiler not found. Please install Visual Studio with C++ workload." -ForegroundColor Red
    exit 1
}

Write-Host "Found compiler: $clPath" -ForegroundColor Green

# Set up environment for cl.exe
$vcvars = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.NativeDesktop -find "VC\Auxiliary\Build\vcvars64.bat" 2>$null
if ($vcvars) {
    cmd /c "`"$vcvars`" && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }
}

# Compile the test
Write-Host "Compiling test..." -ForegroundColor Yellow
& $clPath /std:c++20 /I "$engineInclude" /I "$projectRoot\HyperionJS\include" "$testFile" "$engineSrc\css_parser.cpp" /link /LIBPATH:"$engineLib" hyperion_css_parser.lib /OUT:"$buildDir\css_parser_test.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host "Compilation failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Compilation successful!" -ForegroundColor Green
Write-Host "Running test..." -ForegroundColor Yellow

# Run the test
& "$buildDir\css_parser_test.exe"

if ($LASTEXITCODE -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
} else {
    Write-Host "Tests failed!" -ForegroundColor Red
    exit 1
}
