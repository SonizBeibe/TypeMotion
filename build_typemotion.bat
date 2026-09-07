@echo off
setlocal enabledelayedexpansion

:: =========================================================
:: build_aegisub.bat
:: Compila automaticamente TypeMotion / Aegisub (meson + ninja)
:: Colocar este archivo en la raiz del repositorio
:: (junto a meson.build)
:: =========================================================

cd /d "%~dp0"

echo.
echo ============================================
echo   TypeMotion / Aegisub - Build automatico
echo ============================================
echo.

:: -----------------------------------------------------------
:: 1) Asegurar que el entorno de MSVC (x64) este cargado.
::    Si ya estas en "x64 Native Tools Command Prompt" esto
::    se detecta y no hace nada. Si no, intenta cargarlo solo.
:: -----------------------------------------------------------
if not defined VSCMD_ARG_TGT_ARCH (
    echo [INFO] Entorno de Visual Studio no detectado, buscando vcvarsall.bat...

    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VSINSTALL=%%i"
        )
    )

    if defined VSINSTALL (
        if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvarsall.bat" (
            echo [INFO] Cargando entorno MSVC desde: !VSINSTALL!
            call "!VSINSTALL!\VC\Auxiliary\Build\vcvarsall.bat" x64
        )
    ) else (
        echo [ERROR] No se pudo localizar vcvarsall.bat automaticamente.
        echo         Ejecuta este .bat desde la consola
        echo         "x64 Native Tools Command Prompt for VS" en su lugar.
        goto :fail
    )
)

:: -----------------------------------------------------------
:: 2) Asegurar que gzip (necesario para el subproyecto ffmpeg)
::    este disponible en el PATH de esta sesion.
:: -----------------------------------------------------------
where gzip >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\Git\usr\bin\gzip.exe" (
        echo [INFO] Agregando gzip de Git al PATH de esta sesion...
        set "PATH=%PATH%;C:\Program Files\Git\usr\bin"
    ) else (
        echo [AVISO] gzip no esta en el PATH y no se encontro en
        echo         "C:\Program Files\Git\usr\bin". Si el build falla
        echo         pidiendo gzip, instalalo o ajusta la ruta en este script.
    )
)

:: -----------------------------------------------------------
:: 3) Configurar (solo si no existe la carpeta build) o
::    reconfigurar si ya existe.
:: -----------------------------------------------------------
if not exist "build" (
    echo.
    echo [INFO] Carpeta "build" no encontrada, configurando proyecto por primera vez...
    meson setup build -Ddefault_library=static --buildtype=release
    if errorlevel 1 goto :fail
) else (
    echo.
    echo [INFO] Carpeta "build" encontrada, se usara la configuracion existente.
    echo        (si cambiaste opciones de meson_options.txt, borra "build" y vuelve a correr este script)
)

:: -----------------------------------------------------------
:: 4) Compilar
:: -----------------------------------------------------------
echo.
echo [INFO] Compilando con ninja...
ninja -C build
if errorlevel 1 goto :fail

echo.
echo ============================================
echo   Build completado correctamente.
echo   Binario: build\aegisub.exe
echo ============================================
goto :end

:fail
echo.
echo ============================================
echo   ERROR: la compilacion fallo.
echo   Revisa el mensaje de error de arriba.
echo ============================================
exit /b 1

:end
pause
endlocal
