@echo off
REM build_vlc.bat
REM Download and setup VLC SDK for Windows
REM This script downloads the official VLC SDK and extracts it for CMake to find

setlocal enabledelayedexpansion

REM Get script directory
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM Parse arguments
set "BUILD_DIR=%~1"
set "VLC_VERSION=3.0.21"
set "FORCE=0"

if "%BUILD_DIR%"=="" (
    echo Usage: build_vlc.bat ^<build-dir^> [--force]
    echo Example: build_vlc.bat build-dev
    exit /b 1
)

if "%~2"=="--force" set "FORCE=1"
if "%~2"=="-f" set "FORCE=1"

REM Configuration
set "CMAKE_BUILD_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
set "VLC_INSTALL_DIR=%CMAKE_BUILD_DIR%\vlc-install"
set "VLC_SDK_DIR=%CMAKE_BUILD_DIR%\vlc-sdk"
set "VLC_SDK_7Z=%CMAKE_BUILD_DIR%\vlc-%VLC_VERSION%-win64.7z"

echo ========================================
echo VLC SDK Setup for Windows
echo ========================================
echo.
echo VLC Version: %VLC_VERSION%
echo Build Directory: %CMAKE_BUILD_DIR%
echo Install Directory: %VLC_INSTALL_DIR%
echo.

REM Create build directory if it doesn't exist
if not exist "%CMAKE_BUILD_DIR%" (
    echo Creating build directory: %CMAKE_BUILD_DIR%
    mkdir "%CMAKE_BUILD_DIR%"
)

REM Check if already set up
if exist "%VLC_INSTALL_DIR%\lib\libvlc.lib" (
    if "%FORCE%"=="0" (
        echo VLC SDK already installed at: %VLC_INSTALL_DIR%
        echo Use --force to re-download
        echo.
        echo Next steps:
        echo   1. Reconfigure CMake: make dev-player
        echo   2. Build m1-player: cmake --build %BUILD_DIR% --config Release
        exit /b 0
    ) else (
        echo Cleaning previous installation...
        rmdir /s /q "%VLC_INSTALL_DIR%" 2>nul
        rmdir /s /q "%VLC_SDK_DIR%" 2>nul
    )
)

echo ========================================
echo Checking for 7-Zip
echo ========================================
echo.

REM Find 7-Zip
set "SEVENZIP="
if exist "C:\Program Files\7-Zip\7z.exe" set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"
if exist "C:\Program Files (x86)\7-Zip\7z.exe" set "SEVENZIP=C:\Program Files (x86)\7-Zip\7z.exe"

if "%SEVENZIP%"=="" (
    echo 7-Zip not found. Attempting to install...
    echo.
    
    REM Try winget first
    where winget >nul 2>&1
    if !errorlevel!==0 (
        echo Installing 7-Zip via winget...
        winget install --id 7zip.7zip --accept-source-agreements --accept-package-agreements --silent >nul 2>&1
        if !errorlevel!==0 (
            set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"
        )
    )
    
    REM Try chocolatey if winget failed
    if "!SEVENZIP!"=="" (
        where choco >nul 2>&1
        if !errorlevel!==0 (
            echo Installing 7-Zip via Chocolatey...
            choco install 7zip -y >nul 2>&1
            if !errorlevel!==0 (
                set "SEVENZIP=C:\Program Files\7-Zip\7z.exe"
            )
        )
    )
    
    REM Check if installation succeeded
    if not exist "!SEVENZIP!" (
        echo.
        echo ========================================
        echo Could not install 7-Zip automatically
        echo ========================================
        echo.
        echo Please install 7-Zip manually:
        echo   Option 1: Download from https://www.7-zip.org/
        echo   Option 2: Run: winget install 7zip.7zip
        echo   Option 3: Run: choco install 7zip
        echo.
        echo Then re-run: make build-vlc
        echo.
        exit /b 1
    )
    echo 7-Zip installed successfully!
)

echo Using 7-Zip: %SEVENZIP%

echo.
echo ========================================
echo Downloading VLC SDK
echo ========================================
echo.

REM Check if already downloaded
set "NEEDS_DOWNLOAD=1"
if exist "%VLC_SDK_7Z%" (
    for %%A in ("%VLC_SDK_7Z%") do set "FILE_SIZE=%%~zA"
    if !FILE_SIZE! GTR 30000000 (
        echo Using cached download: %VLC_SDK_7Z%
        set "NEEDS_DOWNLOAD=0"
    ) else (
        echo Cached file appears incomplete, re-downloading...
        del /f "%VLC_SDK_7Z%" 2>nul
    )
)

if "%NEEDS_DOWNLOAD%"=="1" (
    REM Mirror URLs to try
    set "MIRRORS[0]=https://mirrors.netix.net/vlc/vlc/%VLC_VERSION%/win64/vlc-%VLC_VERSION%-win64.7z"
    set "MIRRORS[1]=https://ftp.Halifax.RWTH-Aachen.DE/videolan/vlc/%VLC_VERSION%/win64/vlc-%VLC_VERSION%-win64.7z"
    set "MIRRORS[2]=https://mirror.init7.net/videolan/vlc/%VLC_VERSION%/win64/vlc-%VLC_VERSION%-win64.7z"
    
    set "DOWNLOAD_SUCCESS=0"
    
    for /L %%i in (0,1,2) do (
        if "!DOWNLOAD_SUCCESS!"=="0" (
            echo Downloading from: !MIRRORS[%%i]!
            echo This may take a few minutes...
            
            REM Use curl (built into Windows 10+)
            curl -L -o "%VLC_SDK_7Z%" "!MIRRORS[%%i]!" --progress-bar 2>nul
            
            if exist "%VLC_SDK_7Z%" (
                for %%A in ("%VLC_SDK_7Z%") do set "FILE_SIZE=%%~zA"
                if !FILE_SIZE! GTR 30000000 (
                    echo Download complete!
                    set "DOWNLOAD_SUCCESS=1"
                ) else (
                    echo   Downloaded file too small, trying next mirror...
                    del /f "%VLC_SDK_7Z%" 2>nul
                )
            ) else (
                echo   Mirror failed, trying next...
            )
        )
    )
    
    if "!DOWNLOAD_SUCCESS!"=="0" (
        echo.
        echo ERROR: Failed to download VLC SDK from all mirrors
        echo.
        echo Please download manually:
        echo   1. Go to: https://www.videolan.org/vlc/download-windows.html
        echo   2. Click '7zip package' link ^(NOT the regular zip!^)
        echo   3. Save as: %VLC_SDK_7Z%
        echo   4. Re-run this script
        exit /b 1
    )
)

echo.
echo ========================================
echo Extracting VLC SDK
echo ========================================
echo.

REM Clean up previous extraction
set "EXTRACTED_DIR=%CMAKE_BUILD_DIR%\vlc-%VLC_VERSION%"
if exist "%EXTRACTED_DIR%" (
    echo Removing previous extraction...
    rmdir /s /q "%EXTRACTED_DIR%"
)
if exist "%VLC_SDK_DIR%" (
    rmdir /s /q "%VLC_SDK_DIR%"
)

echo Extracting to: %VLC_SDK_DIR%
"%SEVENZIP%" x "%VLC_SDK_7Z%" -o"%CMAKE_BUILD_DIR%" -y >nul
if errorlevel 1 (
    echo.
    echo ERROR: Failed to extract VLC SDK
    echo Removing corrupted download...
    del /f "%VLC_SDK_7Z%" 2>nul
    echo Please try again: make build-vlc
    exit /b 1
)

REM Rename extracted directory
if exist "%EXTRACTED_DIR%" (
    ren "%EXTRACTED_DIR%" vlc-sdk
    echo Extraction complete
) else (
    echo ERROR: Expected directory not found: %EXTRACTED_DIR%
    exit /b 1
)

REM Verify SDK folder exists
if not exist "%VLC_SDK_DIR%\sdk" (
    echo.
    echo ERROR: SDK folder not found in VLC package!
    echo The downloaded package may be the wrong type.
    exit /b 1
)
echo SDK folder found

echo.
echo ========================================
echo Setting up VLC for CMake
echo ========================================
echo.

REM Create directory structure
mkdir "%VLC_INSTALL_DIR%\include" 2>nul
mkdir "%VLC_INSTALL_DIR%\lib" 2>nul
mkdir "%VLC_INSTALL_DIR%\bin" 2>nul
mkdir "%VLC_INSTALL_DIR%\lib\vlc\plugins" 2>nul

REM Copy SDK headers
if exist "%VLC_SDK_DIR%\sdk\include" (
    echo Copying headers...
    xcopy /s /e /y /q "%VLC_SDK_DIR%\sdk\include\*" "%VLC_INSTALL_DIR%\include\" >nul
)

REM Copy SDK libraries
if exist "%VLC_SDK_DIR%\sdk\lib" (
    echo Copying libraries...
    xcopy /s /e /y /q "%VLC_SDK_DIR%\sdk\lib\*" "%VLC_INSTALL_DIR%\lib\" >nul
)

REM Copy DLLs
echo Copying runtime DLLs...
if exist "%VLC_SDK_DIR%\libvlc.dll" (
    copy /y "%VLC_SDK_DIR%\libvlc.dll" "%VLC_INSTALL_DIR%\bin\" >nul
    echo   Copied: libvlc.dll
)
if exist "%VLC_SDK_DIR%\libvlccore.dll" (
    copy /y "%VLC_SDK_DIR%\libvlccore.dll" "%VLC_INSTALL_DIR%\bin\" >nul
    echo   Copied: libvlccore.dll
)

REM Copy plugins
if exist "%VLC_SDK_DIR%\plugins" (
    echo Copying VLC plugins...
    xcopy /s /e /y /q "%VLC_SDK_DIR%\plugins\*" "%VLC_INSTALL_DIR%\lib\vlc\plugins\" >nul
    
    REM Count plugins
    set "PLUGIN_COUNT=0"
    for /r "%VLC_INSTALL_DIR%\lib\vlc\plugins" %%f in (*.dll) do set /a PLUGIN_COUNT+=1
    echo   Copied !PLUGIN_COUNT! plugins
)

echo.
echo ========================================
echo Verifying Installation
echo ========================================
echo.

set "ALL_FOUND=1"

if exist "%VLC_INSTALL_DIR%\include\vlc\vlc.h" (
    echo   [OK] include\vlc\vlc.h
) else (
    echo   [MISSING] include\vlc\vlc.h
    set "ALL_FOUND=0"
)

if exist "%VLC_INSTALL_DIR%\lib\libvlc.lib" (
    echo   [OK] lib\libvlc.lib
) else (
    echo   [MISSING] lib\libvlc.lib
    set "ALL_FOUND=0"
)

if exist "%VLC_INSTALL_DIR%\lib\libvlccore.lib" (
    echo   [OK] lib\libvlccore.lib
) else (
    echo   [MISSING] lib\libvlccore.lib
    set "ALL_FOUND=0"
)

if exist "%VLC_INSTALL_DIR%\bin\libvlc.dll" (
    echo   [OK] bin\libvlc.dll
) else (
    echo   [MISSING] bin\libvlc.dll
    set "ALL_FOUND=0"
)

if exist "%VLC_INSTALL_DIR%\bin\libvlccore.dll" (
    echo   [OK] bin\libvlccore.dll
) else (
    echo   [MISSING] bin\libvlccore.dll
    set "ALL_FOUND=0"
)

if "%ALL_FOUND%"=="0" (
    echo.
    echo ERROR: Some required files are missing!
    exit /b 1
)

echo.
echo ========================================
echo VLC SDK Setup Complete!
echo ========================================
echo.
echo VLC SDK installed to: %VLC_INSTALL_DIR%
echo.
echo Next steps:
echo   1. Reconfigure CMake:
echo      cmake m1-player -Bm1-player/%BUILD_DIR%
echo      OR: make dev-player
echo.
echo   2. Build m1-player:
echo      cmake --build m1-player/%BUILD_DIR% --config Release
echo.

endlocal
exit /b 0

