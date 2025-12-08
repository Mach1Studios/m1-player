@echo off
REM bundle_vlc_deps.bat
REM Bundle VLC dependencies into the application output directory
REM This script copies VLC DLLs and plugins to the build output

setlocal enabledelayedexpansion

REM Parse arguments
set "OUTPUT_DIR=%~1"
set "VLC_INSTALL_DIR=%~2"

if "%OUTPUT_DIR%"=="" (
    echo Usage: bundle_vlc_deps.bat ^<output-dir^> ^<vlc-install-dir^>
    echo Example: bundle_vlc_deps.bat build-dev\Debug vlc-install
    exit /b 1
)

if "%VLC_INSTALL_DIR%"=="" (
    echo Usage: bundle_vlc_deps.bat ^<output-dir^> ^<vlc-install-dir^>
    echo Example: bundle_vlc_deps.bat build-dev\Debug vlc-install
    exit /b 1
)

echo ========================================
echo Bundling VLC Dependencies (Windows)
echo ========================================
echo.
echo Output Directory: %OUTPUT_DIR%
echo VLC Install Dir: %VLC_INSTALL_DIR%
echo.

REM Verify paths exist
if not exist "%OUTPUT_DIR%" (
    echo ERROR: Output directory does not exist: %OUTPUT_DIR%
    exit /b 1
)

if not exist "%VLC_INSTALL_DIR%" (
    echo ERROR: VLC install directory does not exist: %VLC_INSTALL_DIR%
    exit /b 1
)

REM Determine source paths
set "VLC_BIN_DIR=%VLC_INSTALL_DIR%\bin"
set "VLC_PLUGINS_SOURCE=%VLC_INSTALL_DIR%\lib\vlc\plugins"

REM If bin doesn't exist, DLLs might be at root level
if not exist "%VLC_BIN_DIR%" (
    set "VLC_BIN_DIR=%VLC_INSTALL_DIR%"
)

REM Destination paths
set "PLUGINS_DEST_DIR=%OUTPUT_DIR%\plugins"

echo ========================================
echo Copying VLC Runtime DLLs
echo ========================================
echo.

set "COPIED_DLLS=0"

if exist "%VLC_BIN_DIR%\libvlc.dll" (
    copy /y "%VLC_BIN_DIR%\libvlc.dll" "%OUTPUT_DIR%\" >nul
    echo   Copied: libvlc.dll
    set /a COPIED_DLLS+=1
) else (
    echo   WARNING: libvlc.dll not found
)

if exist "%VLC_BIN_DIR%\libvlccore.dll" (
    copy /y "%VLC_BIN_DIR%\libvlccore.dll" "%OUTPUT_DIR%\" >nul
    echo   Copied: libvlccore.dll
    set /a COPIED_DLLS+=1
) else (
    echo   WARNING: libvlccore.dll not found
)

echo.
echo ========================================
echo Copying VLC Plugins
echo ========================================
echo.

set "TOTAL_PLUGINS=0"

if exist "%VLC_PLUGINS_SOURCE%" (
    echo Source: %VLC_PLUGINS_SOURCE%
    echo Destination: %PLUGINS_DEST_DIR%
    echo.
    
    REM Create plugins directory (remove old one first)
    if exist "%PLUGINS_DEST_DIR%" (
        rmdir /s /q "%PLUGINS_DEST_DIR%"
    )
    mkdir "%PLUGINS_DEST_DIR%"
    
    REM Copy all plugins
    xcopy /s /e /y /q "%VLC_PLUGINS_SOURCE%\*" "%PLUGINS_DEST_DIR%\" >nul
    
    REM Count plugins
    for /r "%PLUGINS_DEST_DIR%" %%f in (*.dll) do set /a TOTAL_PLUGINS+=1
    echo   Total plugins copied: !TOTAL_PLUGINS!
    
    REM Copy plugins.dat if it exists
    if exist "%VLC_PLUGINS_SOURCE%\plugins.dat" (
        copy /y "%VLC_PLUGINS_SOURCE%\plugins.dat" "%PLUGINS_DEST_DIR%\" >nul
        echo   Copied: plugins.dat (plugin cache)
    )
) else (
    echo   WARNING: VLC plugins not found at %VLC_PLUGINS_SOURCE%
    echo   Video playback may not work correctly
)

echo.
echo ========================================
echo Verification
echo ========================================
echo.

set "ALL_PRESENT=1"

if exist "%OUTPUT_DIR%\libvlc.dll" (
    echo   [OK] libvlc.dll
) else (
    echo   [MISSING] libvlc.dll
    set "ALL_PRESENT=0"
)

if exist "%OUTPUT_DIR%\libvlccore.dll" (
    echo   [OK] libvlccore.dll
) else (
    echo   [MISSING] libvlccore.dll
    set "ALL_PRESENT=0"
)

if exist "%PLUGINS_DEST_DIR%" (
    echo   [OK] plugins/ (!TOTAL_PLUGINS! DLLs)
) else (
    echo   [MISSING] plugins/
    set "ALL_PRESENT=0"
)

echo.
if "%ALL_PRESENT%"=="1" (
    echo ========================================
    echo VLC Dependencies Bundled Successfully!
    echo ========================================
    echo.
    echo Copied %COPIED_DLLS% DLLs + !TOTAL_PLUGINS! plugins to:
    echo   %OUTPUT_DIR%
) else (
    echo ========================================
    echo WARNING: Some files may be missing!
    echo ========================================
    exit /b 1
)

endlocal
exit /b 0

