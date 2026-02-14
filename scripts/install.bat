@echo off
set "PROJECT_ROOT=%~dp0.."

echo Installing Ry to C:\ry...
mkdir "C:\ry\bin" 2>nul
mkdir "C:\ry\lib" 2>nul

copy /Y "%PROJECT_ROOT%\bin\ry.exe" "C:\ry\bin\"
copy /Y "%PROJECT_ROOT%\lib\*.dll" "C:\ry\lib\"

:: Add the bin folder to the path
setx /M PATH "%PATH%;C:\ry\bin"

echo Ry installation complete!
pause