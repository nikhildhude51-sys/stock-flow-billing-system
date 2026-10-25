@echo off
title Inventory Management and Billing System
echo ========================================================
echo  Starting Inventory Management and Billing System
echo  C++ Crow Backend + Web Frontend
echo ========================================================
echo.
echo Opening browser at http://localhost:8080 ...
start http://localhost:8080
echo Starting backend server on port 8080 (Press Ctrl+C to stop)...
build\server.exe 8080
pause
