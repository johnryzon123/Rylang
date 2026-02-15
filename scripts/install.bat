@echo off
:: Use %~dp0 to get the directory where the script is located
set "SOURCE_BIN=%~dp0bin"
set "SOURCE_LIB=%~dp0lib"
set "INSTALL_DIR=C:\ry"

echo %BOLD%--- Ry Windows Installer ---%RESET%

:: Check for Admin Rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Please run this script as Administrator.
    pause
    exit /b 1
)

:: Create directories
echo Creating directories at %INSTALL_DIR%...
if not exist "%INSTALL_DIR%\bin" mkdir "%INSTALL_DIR%\bin"
if not exist "%INSTALL_DIR%\lib" mkdir "%INSTALL_DIR%\lib"

:: Copy files (using /Y to overwrite)
echo Copying Ry binaries...
copy /Y "%SOURCE_BIN%\ry.exe" "%INSTALL_DIR%\bin\"
if exist "%SOURCE_LIB%\*.dll" (
    copy /Y "%SOURCE_LIB%\*.dll" "%INSTALL_DIR%\lib\"
)

:: Add to PATH
:: Check if C:\ry\bin is already in the path before adding it
echo Checking PATH configuration...
echo %PATH% | findstr /I /C:";%INSTALL_DIR%\bin" >nul
if %errorlevel% neq 0 (
    echo Adding %INSTALL_DIR%\bin to System PATH...
    setx /M PATH "%PATH%;%INSTALL_DIR%\bin"
) else (
    echo %INSTALL_DIR%\bin is already in PATH.
)

echo.
echo Ry installation complete! You can now type 'ry' in a new CMD window.
pause