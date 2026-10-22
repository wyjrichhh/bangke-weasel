@echo off
setlocal
cd /d %~dp0..

if not exist env.bat copy env.bat.template env.bat
call env.bat
if not defined BANGKE_ROOT set BANGKE_ROOT=%CD%
if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=1
if not defined VERSION_PATCH set VERSION_PATCH=0
if not defined BANGKE_BUILD set BANGKE_BUILD=0
rem Non-release builds use the same git-count label as build.bat.
rem This is a binary build label; MSI upgrades compare only the first three fields.
if not defined RELEASE_BUILD (
  git --version >nul 2>&1
  if not errorlevel 1 (
    for /f "delims=" %%i in ('git rev-list HEAD --count') do set BANGKE_BUILD=%%i
  )
)
if not defined QT_DIR set QT_DIR=C:\Libraries\Qt\6.8.3\msvc2022_64
if not exist "%QT_DIR%\lib\cmake\Qt6" (
  echo Error: Qt6 not found at %QT_DIR%.
  exit /b 1
)
where cmake >nul 2>&1 || set PATH=%DEVTOOLS_PATH%%PATH%

set BANGKE_OUT=%BANGKE_ROOT%\output
cmake -S installer\BangkeInstaller -B installer\BangkeInstaller\build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=%QT_DIR% -DVERSION_MAJOR=%VERSION_MAJOR% -DVERSION_MINOR=%VERSION_MINOR% -DVERSION_PATCH=%VERSION_PATCH% -DVERSION_BUILD=%BANGKE_BUILD%
if errorlevel 1 exit /b 1
cmake --build installer\BangkeInstaller\build --config Release
if errorlevel 1 exit /b 1
echo Installer UI: output\BangkeInstaller.exe (shares Qt DLLs with BangkeSettings)
exit /b 0
