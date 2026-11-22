Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   APEX HFT - LAUNCHING LIVE BINANCE TERMINAL & ENGINE   " -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Start Python Local HTTP Server on port 8080
$server = Start-Process python -ArgumentList "-m", "http.server", "8080", "--directory", "web" -PassThru -NoNewWindow
Write-Host "[+] Local Web Server running at: http://localhost:8080" -ForegroundColor Green

# 2. Open default browser
Start-Sleep -Milliseconds 600
Start-Process "http://localhost:8080"
Write-Host "[+] Browser opened!" -ForegroundColor Green

# 3. Start C++ Matching Engine in Live Streaming Mode
Write-Host "[+] Starting C++ Matching Engine in Live Mode (Press Ctrl+C to stop)..." -ForegroundColor Yellow
try {
    .\build\bin\simulator.exe 0
} finally {
    Stop-Process -Id $server.Id -ErrorAction SilentlyContinue
    Write-Host "`n[+] Simulation and Web Server closed cleanly." -ForegroundColor Cyan
}
