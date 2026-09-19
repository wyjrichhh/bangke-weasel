@echo off

setlocal

if not exist env.bat copy env.bat.template env.bat

if exist env.bat call env.bat

if not defined BANGKE_ROOT set BANGKE_ROOT=%CD%

if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=1
if not defined VERSION_PATCH set VERSION_PATCH=0

if not defined BANGKE_VERSION set BANGKE_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
if not defined BANGKE_BUILD set BANGKE_BUILD=0

rem use numeric build version for release build
set PRODUCT_VERSION=%BANGKE_VERSION%.%BANGKE_BUILD%
rem for non-release build, try to use git commit hash as product build version
if not defined RELEASE_BUILD (
  rem check if git is installed and available, then get the short commit id of head
  git --version >nul 2>&1
  if not errorlevel 1 (
    rem FILEVERSION 第四段取总提交数:标签匹配法在无匹配标签时静默得 0,
    rem 同版本号重打会被 MSI 按"版本不更高"跳过(bangke.dll 陈旧文件之坑)
    for /f "delims=" %%i in ('git rev-list HEAD --count') do (
      set BANGKE_BUILD=%%i
    )
    rem get short commmit id of head
    for /F %%i in ('git rev-parse --short HEAD') do (set PRODUCT_VERSION=%BANGKE_VERSION%.%BANGKE_BUILD%.%%i)
  )
)

rem FILE_VERSION is always 4 numbers; same as PRODUCT_VERSION in release build
if not defined FILE_VERSION set FILE_VERSION=%BANGKE_VERSION%.%BANGKE_BUILD%

echo PRODUCT_VERSION=%PRODUCT_VERSION%
echo BANGKE_VERSION=%BANGKE_VERSION%
echo BANGKE_BUILD=%BANGKE_BUILD%
echo BANGKE_ROOT=%BANGKE_ROOT%
echo BANGKE_BUNDLED_RECIPES=%BANGKE_BUNDLED_RECIPES%
echo.

if defined GITHUB_ENV (
	setlocal enabledelayedexpansion
	echo git_ref_name=%PRODUCT_VERSION%>>!GITHUB_ENV!
)

if defined BOOST_ROOT (
  if exist "%BOOST_ROOT%\boost" goto boost_found
)
echo Error: Boost not found! Please set BOOST_ROOT in env.bat.
exit /b 1

:boost_found
echo BOOST_ROOT=%BOOST_ROOT%
echo.

if not defined BJAM_TOOLSET (
  rem the number actually means platform toolset, not %VisualStudioVersion%
  set BJAM_TOOLSET=msvc-14.2
)

if not defined PLATFORM_TOOLSET (
  set PLATFORM_TOOLSET=v142
)

if defined DEVTOOLS_PATH set PATH=%DEVTOOLS_PATH%%PATH%

set build_config=Release
set build_option=/t:Build
set build_boost=0
set boost_build_variant=release
set build_data=0
set build_opencc=0
set build_rime=0
set rime_build_variant=release
set build_bangke=0
set build_settings=0

rem parse the command line options
:parse_cmdline_options
  if "%1" == "" goto end_parsing_cmdline_options
  if "%1" == "debug" (
    set build_config=Debug
    set boost_build_variant=debug
    set rime_build_variant=debug
  )
  if "%1" == "release" (
    set build_config=Release
    set boost_build_variant=release
    set rime_build_variant=release
  )
  if "%1" == "rebuild" set build_option=/t:Rebuild
  if "%1" == "boost" set build_boost=1
  if "%1" == "data" set build_data=1
  if "%1" == "opencc" set build_opencc=1
  if "%1" == "rime" set build_rime=1
  if "%1" == "librime" set build_rime=1
  if "%1" == "bangke" set build_bangke=1
  if "%1" == "settings" set build_settings=1
  if "%1" == "all" (
    set build_boost=1
    set build_data=1
    set build_opencc=1
    set build_rime=1
    set build_bangke=1
  )
  shift
  goto parse_cmdline_options
:end_parsing_cmdline_options

if %build_bangke% == 0 (
if %build_boost% == 0 (
if %build_data% == 0 (
if %build_opencc% == 0 (
if %build_rime% == 0 (
if %build_settings% == 0 (
  set build_bangke=1
))))))

rem quit BangkeServer.exe before building
cd /d %BANGKE_ROOT%
if exist output\BangkeServer.exe (
  output\BangkeServer.exe /q
)
  rem AI 预测模型:models\predict_models 不入库,存在才随产物打包
  if exist models\predict_models (
    if not exist output\predict_models mkdir output\predict_models
    xcopy /E /I /Y models\predict_models output\predict_models >nul
  )
  rem weasel.yaml 源头在 data\(入库),覆盖 plum 产物,保证默认样式随仓库走
  if exist output\data copy /Y data\weasel.yaml output\data\ >nul

rem build booost
if %build_boost% == 1 (
  call :build_boost
  if errorlevel 1 exit /b 1
  cd /d %BANGKE_ROOT%
)

rem -------------------------------------------------------------------------
rem build x64 librime
if %build_rime% == 1 (
  rem ai-predict: CTranslate2 静态库（/MT），存在才启用
  if exist C:\dev\ct2-install\lib\ctranslate2.lib set CTRANSLATE2_ROOT=C:\dev\ct2-install
  if not exist librime\build.bat (
    git submodule update --init --recursive
  )
  rem mac 上打的 tar 会带 AppleDouble 元数据(._*),解包后混进插件源码目录,被各插件
  rem CMakeLists.txt 的源码 glob 当 .cc 收进去,必然编译失败且报的是语法错,查起来
  rem 离题很远。幂等:没有匹配就什么都不做。
  del /f /q /s "librime\plugins\._*" >nul 2>&1

  rem 插件是 librime/plugins/CMakeLists.txt 扫目录自动接入的:源码不在就只有目录名,
  rem rime.dll 会静默编成没有 AI 的样子,直到用户发现候选栏再无 AI。
  rem 注意这里查的是 CMakeLists.txt 而非目录——librime 里 plugins/ai-predict 记的是
  rem 符号链接,Windows 检出后会退化成一个内容为路径的普通文件。
  if not exist librime\plugins\ai-predict\CMakeLists.txt (
    echo [build.bat] librime\plugins\ai-predict 里没有插件源码。
    echo [build.bat] 先跑 scripts\fetch-ai-predict.ps1 铺料,否则本次编出的 rime.dll 不含 AI 预测。
    exit /b 1
  )
  rem librime-lua 同样不在 librime 仓库里,各平台各自铺(名字 mac 上叫 lua、Windows 上叫
  rem librime-lua,所以按名字里带 lua 找)。雾凇拼音的日期/农历/数字等 lua_translator 全
  rem 靠它;少了它 rime.dll 照样编得出来,只是那些组件全部加载失败——与 AI 插件同一类
  rem 静默失败,故一并挡在这里。
  set BANGKE_LUA_PLUGIN=
  for /d %%d in (librime\plugins\*lua*) do set BANGKE_LUA_PLUGIN=%%d
  if not defined BANGKE_LUA_PLUGIN (
    echo [build.bat] librime\plugins 下没有 lua 插件。
    echo [build.bat] 雾凇拼音依赖它,少了它 rime.dll 里不会有 lua_translator。
    exit /b 1
  )
  cd %BANGKE_ROOT%\librime
  rem clean cache before building
  for %%a in ( build dist lib ^
    deps\glog\build ^
    deps\googletest\build ^
    deps\leveldb\build ^
    deps\marisa-trie\build ^
    deps\opencc\build ^
    deps\yaml-cpp\build ) do (
      if exist %%a rd /s /q %%a
  )

  rem build x64 librime
  set ARCH=x64
  call :build_librime_platform x64 %BANGKE_ROOT%\lib64 %BANGKE_ROOT%\output
  rem clean the modified file
  rem git checkout .
  rem git submodule foreach git checkout .
)

rem -------------------------------------------------------------------------
if %build_bangke% == 1 (
  if not exist output\data\essay.txt (
    set build_data=1
  )
  if not exist output\data\opencc\TSCharacters.ocd* (
    set build_opencc=1
  )
)
if %build_data% == 1 call :build_data
if %build_opencc% == 1 call :build_opencc_data

if %build_bangke% == 0 goto after_bangke

cd /d %BANGKE_ROOT%

set BANGKE_PROJECT_PROPERTIES=BOOST_ROOT^
  PLATFORM_TOOLSET^
  VERSION_MAJOR^
  VERSION_MINOR^
  VERSION_PATCH^
  PRODUCT_VERSION^
  FILE_VERSION^
  BANGKE_BUILD

cscript.exe render.js bangke.props %BANGKE_PROJECT_PROPERTIES%

del msbuild*.log

rem msbuild 只在交互式 VS 提示符的 PATH 里,sshd 会话不会刷新 PATH,DEVTOOLS_PATH 也
rem 没带上它。先看 PATH,找不到就用 VS 自带的 vswhere 从安装目录里取。
set MSBUILD=msbuild.exe
where msbuild.exe >nul 2>&1
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do set "MSBUILD=%%i"
)

if defined SDKVER set build_sdk_option=/p:WindowsTargetPlatformVersion=%SDKVER%
if not defined SDKVER set build_sdk_option=


"%MSBUILD%" bangke.sln %build_option% /p:Configuration=%build_config% /p:Platform="x64" /fl2 %build_sdk_option%
if errorlevel 1 goto error

:after_bangke
if %build_settings% == 1 (
  call :build_settings
  if errorlevel 1 goto error
)

goto end

rem -------------------------------------------------------------------------
rem build boost
:build_boost
  set BJAM_OPTIONS_COMMON=-j%NUMBER_OF_PROCESSORS%^
    --with-thread^
    define=BOOST_USE_WINAPI_VERSION=0x0603^
    toolset=%BJAM_TOOLSET%^
    link=static^
    runtime-link=static^
    --build-type=complete

  set BJAM_OPTIONS_X64=%BJAM_OPTIONS_COMMON%^
    architecture=x86^
    address-model=64

  cd /d %BOOST_ROOT%
  if not exist b2.exe call bootstrap.bat
  if errorlevel 1 goto error
  b2 %BJAM_OPTIONS_X64% stage %BOOST_COMPILED_LIBS%
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
:build_data
  copy %BANGKE_ROOT%\LICENSE.txt output\
  copy %BANGKE_ROOT%\README.md output\README.txt
  copy %BANGKE_ROOT%\plum\rime-install.bat output\
  set plum_dir=plum
  set rime_dir=output/data
  set WSLENV=plum_dir:rime_dir
  bash plum/rime-install %BANGKE_BUNDLED_RECIPES%
  call :install_rime_ice
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
:install_rime_ice
rem 雾凇拼音(rime-ice):tarball 预置仓库根(不入库,网络不稳,离线安装)
if not exist rime-ice.tar.gz (
  echo [warn] rime-ice.tar.gz missing, skip
  exit /b 0
)
if exist rime-ice-x rmdir /s /q rime-ice-x
mkdir rime-ice-x
tar -xzf rime-ice.tar.gz -C rime-ice-x --strip-components=1 2>nul
rem others/ 内 GBK 文件名会解压报错,所需文件不受影响
xcopy /E /I /Y rime-ice-x\cn_dicts output\data\cn_dicts >nul
xcopy /E /I /Y rime-ice-x\en_dicts output\data\en_dicts >nul
xcopy /E /I /Y rime-ice-x\lua output\data\lua >nul
xcopy /E /I /Y rime-ice-x\opencc output\data\opencc >nul
copy /Y rime-ice-x\rime_ice.schema.yaml output\data\ >nul
copy /Y rime-ice-x\rime_ice.dict.yaml output\data\ >nul
copy /Y rime-ice-x\melt_eng.dict.yaml output\data\ >nul
copy /Y rime-ice-x\radical_pinyin.dict.yaml output\data\ >nul
copy /Y rime-ice-x\symbols_v.yaml output\data\ >nul
copy /Y rime-ice-x\symbols_caps_v.yaml output\data\ >nul
copy /Y rime-ice-x\custom_phrase.txt output\data\ >nul
rem 方案只保留 明月/雾凇(melt_eng/radical 仅挂词库,不装独立 schema)
for %%f in (output\data\*.schema.yaml) do (
  echo %%~nf | findstr /r /c:"^luna_pinyin$" /c:"^rime_ice$" >nul || del "%%f"
)
copy /Y data\weasel.yaml output\data\ >nul
rem 拼音注释内容源:开关走 style/comment_font_point(渲染端门控),
rem 两方案都生成 spelling_hints,luna 上游默认没有,缺则补
powershell -NoProfile -Command "$f='output\data\luna_pinyin.schema.yaml'; $t=[IO.File]::ReadAllText($f); if(-not $t.Contains('spelling_hints')){ $t=$t -replace '(?m)^translator:\r?\n', ('translator:'+[char]10+'  spelling_hints: 8'+[char]10) }; [IO.File]::WriteAllText($f,$t,(New-Object Text.UTF8Encoding($false)))"
exit /b 0

:build_opencc_data
  if not exist %BANGKE_ROOT%\librime\share\opencc\TSCharacters.ocd2 (
    cd %BANGKE_ROOT%\librime
    call build.bat deps %rime_build_variant%
    if errorlevel 1 goto error
  )
  cd %BANGKE_ROOT%
  if not exist output\data\opencc mkdir output\data\opencc
  copy %BANGKE_ROOT%\librime\share\opencc\*.* output\data\opencc\
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
rem %1 : ARCH
rem %2 : push | pop , push to backup when pop to restore
:stash_build
  pushd %BANGKE_ROOT%\librime
  for %%a in ( build dist lib ^
    deps\glog\build ^
    deps\googletest\build ^
    deps\leveldb\build ^
    deps\marisa-trie\build ^
    deps\opencc\build ^
    deps\yaml-cpp\build ) do (
    if "%2"=="push" (
      if exist %%a  move %%a %%a_%1 
    )
    if "%2"=="pop" (
      if exist %%a_%1  move %%a_%1 %%a 
    )
  )
  popd
  exit /b

rem ---------------------------------------------------------------------------
rem %1 : ARCH
rem %2 : target_path of rime.lib, base %BANGKE_ROOT% or abs path
rem %3 : target_path of rime.dll, base %BANGKE_ROOT% or abs path
:build_librime_platform
  rem restore backuped %1 build
  call :stash_build %1 pop

  cd %BANGKE_ROOT%\librime
  if not exist env.bat (
    copy %BANGKE_ROOT%\env.bat env.bat
  )
  if not exist lib\opencc.lib (
    call build.bat deps %rime_build_variant%
    if errorlevel 1 (
      call :stash_build %1 push
      goto error
    )
  )
  call build.bat %rime_build_variant%
  if errorlevel 1 (
    call :stash_build %1 push
    goto error
  )

  cd %BANGKE_ROOT%\librime
  call :stash_build %1 push

  copy /Y %BANGKE_ROOT%\librime\dist_%1\include\rime_*.h %BANGKE_ROOT%\include\
  if errorlevel 1 goto error
  copy /Y %BANGKE_ROOT%\librime\dist_%1\lib\rime.lib %2\
  if errorlevel 1 goto error
  copy /Y %BANGKE_ROOT%\librime\dist_%1\lib\rime.dll %3\
  if errorlevel 1 goto error

  exit /b
rem ---------------------------------------------------------------------------

:build_settings
  cd /d %BANGKE_ROOT%
  if not defined QT_DIR set QT_DIR=C:\Libraries\Qt\6.8.3\msvc2022_64
  if not exist "%QT_DIR%\lib\cmake\Qt6" (
    echo Error: Qt6 not found at %QT_DIR%. Set QT_DIR in env.bat.
    exit /b 1
  )
  where cmake >nul 2>&1 || set PATH=%DEVTOOLS_PATH%%PATH%
  cmake -S BangkeSettings -B BangkeSettings\build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=%QT_DIR% -DVERSION_MAJOR=%VERSION_MAJOR% -DVERSION_MINOR=%VERSION_MINOR% -DVERSION_PATCH=%VERSION_PATCH% -DVERSION_BUILD=%BANGKE_BUILD%
  if errorlevel 1 goto error
  cmake --build BangkeSettings\build --config %build_config%
  if errorlevel 1 goto error
  copy /Y BangkeSettings\build\%build_config%\BangkeSettings.exe output\
  if errorlevel 1 goto error
  "%QT_DIR%\bin\windeployqt" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime output\BangkeSettings.exe
  if errorlevel 1 goto error
  exit /b
:error

cd %BANGKE_ROOT%
echo error building bangke...
exit /b 1

:end
cd %BANGKE_ROOT%
