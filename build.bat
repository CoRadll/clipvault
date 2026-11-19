@echo off
setlocal

call "D:\Visual studio\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64.bat not found or failed
    exit /b 1
)

cd /d "%~dp0"

if /i "%~1"=="clean" (
    echo [clean] removing build directory...
    if exist build rmdir /s /q build
)

echo [1/2] configure...
cmake --preset debug
if errorlevel 1 (
    echo [ERROR] configure failed
    exit /b 1
)

echo [2/2] build...
cmake --build --preset debug
if errorlevel 1 (
    echo [ERROR] build failed
    exit /b 1
)

echo.
echo [OK] build complete

if /i "%~1"=="test" (
    echo.
    echo === running tests ===
    build\debug\clipvault_tests.exe
    if errorlevel 1 (
        echo [ERROR] tests failed
        exit /b 1
    )
)

endlocal
