$ErrorActionPreference = "Stop"
cmake -S . -B build -A x64
cmake --build build --config Release
Write-Host "Built: build\Release\mnf_tool.exe"
