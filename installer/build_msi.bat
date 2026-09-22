@echo off
setlocal
cd /d %~dp0..

if not exist env.bat copy env.bat.template env.bat
call env.bat
if not defined BANGKE_ROOT set BANGKE_ROOT=%CD%
if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=1
if not defined VERSION_PATCH set VERSION_PATCH=0
set BANGKE_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%

set WIXEXE=%USERPROFILE%\.dotnet\tools\wix.exe
if not exist "%WIXEXE%" (
  echo Error: wix not found. Install with: dotnet tool install --global wix
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File installer\harvest.ps1 -OutputDir output -OutFile installer\Files.wxs
if errorlevel 1 goto error

"%WIXEXE%" extension remove -g WixToolset.UI.wixext >nul 2>&1
"%WIXEXE%" extension remove -g WixToolset.Util.wixext >nul 2>&1
"%WIXEXE%" extension add -g WixToolset.UI.wixext/5.0.2
"%WIXEXE%" extension add -g WixToolset.Util.wixext/5.0.2
if errorlevel 1 goto error

"%WIXEXE%" build -arch x64 -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext installer\Bangke.wxs installer\Files.wxs ^
  -d Version=%BANGKE_VERSION% ^
  -d OutputDir=%BANGKE_ROOT%\output ^
  -d ResourceDir=%BANGKE_ROOT%\resource ^
  -o output\BangkeSetup-%BANGKE_VERSION%-x64.msi
if errorlevel 1 goto error

echo MSI: output\BangkeSetup-%BANGKE_VERSION%-x64.msi
exit /b 0

:error
echo error building msi...
exit /b 1
