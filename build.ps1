$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$env:Path = "$userPath;$machinePath"

Write-Output "--- Configuring with CMake ---"
& cmake -B build -G "MinGW Makefiles"
if ($LASTEXITCODE -ne 0) {
    Write-Output "CMake configuration failed!"
    exit $LASTEXITCODE
}

Write-Output "--- Building Server Binary ---"
& cmake --build build
if ($LASTEXITCODE -ne 0) {
    Write-Output "Build failed!"
    exit $LASTEXITCODE
}

Write-Output "--- Build Completed Successfully! ---"
Get-Item build\server.exe
