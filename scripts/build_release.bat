@echo off
setlocal

set "PROJECT_DIR=%~dp0.."

if not defined QT_DIR set "QT_DIR=C:\Qt\6.8.3\msvc2022_64"
if not defined CMAKE_EXE set "CMAKE_EXE=cmake"
if not defined VSDEVCMD set "VSDEVCMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"

if not exist "%VSDEVCMD%" (
    echo Visual Studio developer command prompt not found: "%VSDEVCMD%"
    echo Set VSDEVCMD to your VsDevCmd.bat path.
    exit /b 1
)

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo Qt deploy tool not found: "%QT_DIR%\bin\windeployqt.exe"
    echo Set QT_DIR to your Qt MSVC kit path, for example C:\Qt\6.8.3\msvc2022_64.
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" -S "%PROJECT_DIR%" -B "%PROJECT_DIR%\build" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build "%PROJECT_DIR%\build" --config Release
if errorlevel 1 exit /b %errorlevel%

"%QT_DIR%\bin\windeployqt.exe" "%PROJECT_DIR%\build\Release\QwenPawTray.exe"
exit /b %errorlevel%
