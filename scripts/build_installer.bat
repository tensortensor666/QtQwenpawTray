@echo off
setlocal

set "PROJECT_DIR=%~dp0.."
set "RELEASE_DIR=%PROJECT_DIR%\build\Release"
set "STAGE_DIR=%PROJECT_DIR%\packaging\stage"
if not defined ISCC_EXE set "ISCC_EXE=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"

call "%PROJECT_DIR%\scripts\build_release.bat"
if errorlevel 1 exit /b %errorlevel%

if not exist "%ISCC_EXE%" (
    for %%I in (ISCC.exe) do set "ISCC_FROM_PATH=%%~$PATH:I"
    if defined ISCC_FROM_PATH set "ISCC_EXE=%ISCC_FROM_PATH%"
)

if not exist "%ISCC_EXE%" (
    echo Inno Setup compiler not found.
    echo Set ISCC_EXE to ISCC.exe or install it with: winget install --id JRSoftware.InnoSetup -e
    exit /b 1
)

if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
robocopy "%RELEASE_DIR%" "%STAGE_DIR%" /MIR /XD logs /XF *.pdb *.ilk
set "ROBOCOPY_EXIT=%ERRORLEVEL%"
if %ROBOCOPY_EXIT% GEQ 8 exit /b %ROBOCOPY_EXIT%

for %%F in (msvcp140.dll msvcp140_1.dll msvcp140_2.dll vcruntime140.dll vcruntime140_1.dll concrt140.dll) do (
    if exist "%SystemRoot%\System32\%%F" copy /Y "%SystemRoot%\System32\%%F" "%STAGE_DIR%\" >nul
)

"%ISCC_EXE%" "%PROJECT_DIR%\packaging\qwenpawtray.iss" /DProjectDir="%PROJECT_DIR%" /DStageDir="%STAGE_DIR%"
exit /b %errorlevel%
