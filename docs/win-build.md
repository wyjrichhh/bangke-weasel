# Windows 构建环境（17 机器）

Phase 0 基线：weasel 0.17.4（d73f629），2026-09-04 全量构建 + 打字验证通过。

## 代码放哪：公开 / 私有是两套

| 目录 | 克隆自 | 用途 |
|---|---|---|
| `C:\dev\bangke` | `wyjrichhh/bangke-weasel`（私有） | **本仓库的构建就在这里** |
| `C:\dev\weasel` | `wyjrichhh/weasel`（公开 fork） | 只作参考：查公开版、比打包脚本。**不要在这里构建本仓库** |

两边在 `dfff7ae` 处分岔：私有仓库是从那儿往下走的 `main`，公开那份停在原地。三个私有
仓库克隆都要凭据，这台机器上没配，所以代码靠 bundle 传：

```bash
# 本机（mac）
git -C bangke-weasel bundle create /tmp/bk.bundle main     # 注意别写 ^<某提交>
scp /tmp/bk.bundle bangke@192.168.101.17:C:/dev/
```

```powershell
# 17 机器
git -C C:\dev\bangke init
git -C C:\dev\bangke fetch C:\dev\bk.bundle main:refs/heads/main
git -C C:\dev\bangke checkout main
git -C C:\dev\bangke submodule init
git -C C:\dev\bangke config submodule.librime.url <librime 的 bundle 或本地克隆>
git -C C:\dev\bangke -c protocol.file.allow=always submodule update --init librime
git -C C:\dev\bangke\librime submodule update --init --recursive   # deps/* 是嵌套子模块
powershell -File C:\dev\bangke\scripts\fetch-ai-predict.ps1 -Source <插件的 bundle 或本地克隆> -Ref main
```

**bundle 别带 `^` 前置提交**：带了的只能喂给已有那段历史的仓库，空仓库会报
`Repository lacks these prerequisite commits`。`librime` 的 `deps/*` 是嵌套子模块，
`submodule update --init librime` 只初始化 librime 自己，少了 `--recursive` 那一步，
构建时报的是 `deps/glog does not appear to contain CMakeLists.txt`。

`env.bat` 两边是同一份 —— 仓库里没有任何地方依赖它的绝对路径（`BANGKE_ROOT` 由
`build.bat` 取 `%CD%`），搬过去即可。

`output\data`（plum 配方 + 雾凇词库 + opencc）是纯数据，可以整份搬过来，省掉走 GitHub 那
一步。**但要注意它会让 `:build_data` 整个不跑**（`build.bat` 只按 `output\data\essay.txt`
在不在来决定）—— 那一步顺带往 `output\` 放的 `README.txt`、`LICENSE.txt` 也就一起没了，
而它们会被 harvest 打进 MSI。搬数据的话记得手工补：

```powershell
Copy-Item <旧树>\output\README.txt <新树>\output\
Copy-Item <新树>\LICENSE.txt <新树>\output\
```

## 工具链

| 组件 | 版本/路径 | 说明 |
|---|---|---|
| VS2022 Community | 17.14.39，MSVC 14.44.35207 | workload NativeDesktop + **ATL v143**（WTL 依赖）；不需要 MFC（见下） |
| Boost | 1.84.0，`C:\Libraries\boost_1_84_0` | 必须用 `archives.boost.io` 官方源码包；GitHub Release 的 7z 是无子模块超项目快照，没有 `boost/` 头文件 |
| cmake | 3.31.7 便携版，`C:\Libraries\cmake-3.31.7-windows-x86_64` | **不能用 4.x**（移除 <3.5 兼容，yaml-cpp 等老依赖全挂） |
| Python | 3.12.8，`C:\Libraries\Python312` | librime `data/CMakeLists.txt` 的 find_package(PythonInterp REQUIRED) 硬依赖 |
| Git / 7-Zip | winget 安装 | git-bash 供 plum 数据打包用 |

`env.bat`（仓库根，gitignored）关键项：`BOOST_ROOT` / `BJAM_TOOLSET=msvc-14.3` / `PLATFORM_TOOLSET=v143` / `CMAKE_GENERATOR="Visual Studio 17 2022"` / `DEVTOOLS_PATH`（Python 和 cmake 的便携目录挂最前，sshd 会话不会刷新 PATH，全靠这个变量）。

## 本地补丁（已提交）

1. **b2 msvc.jam**（`C:\Libraries\boost_1_84_0\tools\build\src\tools\msvc.jam`，**不进 git**，重建 boost 环境时重打）：首分支 `if [ MATCH "(14.3)"...]` 改为 `(14.[1-9])`。原因：b2 4.10 的版本正则不认 MSVC 14.44，走老分支拼出不存在的 `bin\Hostx64\vcvarsall.bat` 依赖目标，全部编译目标被 skip。
2. **afxres.h → windows.h**（`BangkeTSF/BangkeTSF.rc`、`BangkeServer/BangkeServer.rc`，已提交）：`.rc` 里的 `afxres.h` 是 MFC 头，官方 CI 的 runner 镜像自带 MFC 所以没暴露；替换成 `windows.h` 后永久去掉 MFC 组件依赖。注意两个 `.rc` 都是 **UTF-16LE**，改完要保编码。

## AI 插件铺料（librime 之外，必须单独放）

插件不在 librime 仓库内。`librime` 里 `plugins/ai-predict` 记的是一个指向仓库外克隆的
符号链接，Windows 检出后会退化成一个 40 字节、内容为路径的普通文件 —— 而
`librime/plugins/CMakeLists.txt` 是按目录扫描接入插件的，扫不到就静默编出一个**没有 AI**
的 rime.dll。所以构建前必须先铺：

```powershell
powershell -File scripts\fetch-ai-predict.ps1                        # 用同级克隆
powershell -File scripts\fetch-ai-predict.ps1 -Source D:\ai.bundle -Ref <branch>
```

`build.bat rime` 会检查 `librime\plugins\ai-predict\CMakeLists.txt`（查文件而非目录，
正是为了识破退化的符号链接），不在就直接失败而不是编出个没有 AI 的版本。

## CTranslate2（AI 插件的推理后端）

插件静态链接 CT2，而 `CTRANSLATE2_ROOT` 是 `build.bat` 写死的
`C:\dev\ct2-install\lib\ctranslate2.lib` 在不在来决定的。缺了它，要么插件配置阶段报
`CTranslate2 not found`，要么退回插件自带的 `make deps`（要拉 `deps/CTranslate2` 子模块
再整个重编一遍）。

这台机器上那份是 CT2 **4.7.1** 从源码静态编的，配置如下：

```bat
cmake -S C:\dev\ct2-src -B C:\dev\ct2-src\build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_INSTALL_PREFIX=C:/dev/ct2-install ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DOPENMP_RUNTIME=NONE -DWITH_CUDA=OFF -DWITH_MKL=OFF -DWITH_DNNL=OFF -DWITH_RUY=ON
cmake --build C:\dev\ct2-src\build --config Release --target install
```

CT2 与 AI 插件静态链接进同一个 rime.dll，因此使用 `/MT`；
`WITH_RUY=ON` 才能让 int8 模型真的按 int8 跑。
Qt 官方共享版使用 `/MD`；安装器和设置程序也使用 `/MD`，设置程序所需的
Boost thread 静态库另外构建 `runtime-link=shared` 变体。`build.bat boost`
现在同时生成 `runtime-link=static,shared`，分别供原生组件与 Qt 程序使用。

## librime-lua 也得单独铺

同一类事：`librime-lua` 同样不在 librime 仓库里，各平台各自铺（mac 上目录名叫 `lua`，
Windows 上叫 `librime-lua`）。**雾凇拼音的日期、农历、数字、计算器等组件全是 lua
translator，少了它 rime.dll 照样编得出来，只是那些组件全部加载失败** —— 症状是那些功能
静默消失，而不是报错。

`build.bat rime` 现在按「`librime\plugins` 下有没有名字带 lua 的目录」挡一道。铺料走上游，
不要从别的检出里拷（那条路依赖旁边恰好有那么一棵树）：

```bat
git clone https://github.com/hchunhui/librime-lua librime\plugins\librime-lua
git -C librime\plugins\librime-lua checkout ad1e4a6
```

## 增量构建的坑：不要直接调 librime\build.bat

外层 `build.bat` 会把 `lib` / `build` / `dist` 和各个 deps 的 `build` 目录改名成
`<名>_x64` 藏起来（`:stash_build push`），下次构建先弹回来（`pop`），这样依赖库不用重编。
**直接调 `librime\build.bat` 会绕过弹出那一步**，此时 `lib` 是空的，cmake 报的是
`Could not find glog library` —— 看着像环境坏了，其实是构建的暂存机制。要增量就照旧走
`build.bat rime`。

## 构建序列

```bat
cd C:\dev\bangke
build.bat boost     :: b2 双架构静态库，约 10 分钟
build.bat rime      :: librime deps + rime.dll，约 30 分钟
build.bat data opencc  :: plum 配方 + opencc 词库（走 GitHub，网络抖时重跑）
build.bat           :: msbuild 全解决方案（x64 + Win32）
build.bat settings  :: Qt 设置程序
```

`build.bat rime` 里有两处防御，都是踩过的坑：先清掉 `plugins/` 下的 AppleDouble
元数据（mac 上打的 tar 会带 `._*`，被源码 glob 当 `.cc` 收进去后报的是一连串离题语法
错），再查插件源码在不在。主件构建时若 PATH 里没有 msbuild，会用 VS 自带的 vswhere
定位 —— sshd 会话不会刷新 PATH，`DEVTOOLS_PATH` 里也没有它。

## 打包：MSI 与单文件 exe

```bat
installer\build_installer.bat              :: 图形安装器 UI -> output\BangkeInstaller.exe
installer\build_msi.bat                    :: -> output\BangkeSetup-<版本>-x64.msi
powershell -File installer\make_boot.ps1   :: -> dist\BangkeSetup-<版本>-win64.exe
```

**要发给用户的是最后那个 exe**：单文件，双击即装。`make_boot.ps1` 把图形安装器、最新
MSI、Qt 运行库、VC 运行库一起嵌进一个原生引导壳。三步有先后：
`make_boot` 是从 `output\` 收载荷的，所以前面几件得先生成。

当前设置程序使用 Qt Quick 默认的 D3D11 后端；`trim_output.ps1` 会移除
仅供 D3D12 使用的 `dxcompiler.dll` / `dxil.dll`，`make_boot.ps1` 将它们视为可选项。
旧文档中“缺少它们必然启动崩溃”的说法已过期。若以后启用 D3D12，须同步调整裁剪和打包清单。

MSI 的升级版本比较仅使用前三段；第四段不能充当递增升级版本，参见
[微软 ProductVersion 文档](https://learn.microsoft.com/windows/win32/msi/productversion)。
`build_msi.bat` 使用与二进制相同的 `BANGKE_BUILD` 作为第四段。正式发布必须升
`VERSION_PATCH` 并重编所有二进制；同版本不同 ProductCode 通过
`AllowSameVersionUpgrades` 接管，同 ProductCode 则修复已安装产品。
禁止用 `REINSTALLMODE=amus` 强制覆盖来代替升级。

安装日志保留在 `%TEMP%\BangkeSetupLogs`。引导壳每次使用独立临时目录，
退出后只清理本次载荷；日志不会随载荷一起删除。

## 开发态安装（免 NSIS）

```powershell
# 管理员会话；server 必须在交互桌面会话里启动，不要从 ssh 拉起
cd C:\dev\bangke\output
Copy-Item .\bangkex64.dll C:\Windows\System32\bangke.dll -Force
cmd /c "set TEXTSERVICE_PROFILE=hans&& C:\Windows\System32\regsvr32.exe /s C:\Windows\System32\bangke.dll"
New-Item HKLM:\SOFTWARE\Bangke -Force
Set-ItemProperty HKLM:\SOFTWARE\Bangke BangkeRoot 'C:\dev\bangke\output'
Set-ItemProperty HKLM:\SOFTWARE\Bangke ServerExecutable 'BangkeServer.exe'
New-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' BangkeServer -PropertyType String -Value 'C:\dev\bangke\output\BangkeServer.exe' -Force
```

（也可直接用 `scripts/dev_install.ps1`，上面是它做的事。）首次部署**不需要**手动跑设置程序——直接启动 `BangkeServer.exe`，`Initialize()` 的 `start_maintenance` 会自动完成首次部署（编译词库约 1-3 分钟）。

## 排查：装完只能打英文，切不过去

先分清是哪一层：

**一、输入列表里有没有它。** 看用户的语言列表：

```powershell
Get-WinUserLanguageList | ForEach-Object { $_.LanguageTag + " : " + ($_.InputMethodTips -join ", ") }
```

zh-Hans-CN 那行应该含 `0804:{9D44BD49-B647-4010-9ADC-16DA253F5CCA}{ED00E0F3-7EB2-4D43-898F-E2DA5ACD15CA}`。
没有的话，是 TSF 注册没落到当前用户头上（`RegisterProfile` 是"往当前用户的列表里加"的
API，以 SYSTEM 身份调就加进了 SYSTEM 的列表；机器层面看着一切正常——`InprocServer32`
在、`LanguageProfile` 的 `Enable=1` 也在）。补一次即可，**要在你的桌面会话里、带管理员
权限**：

```powershell
C:\Windows\System32\regsvr32.exe /s C:\Windows\System32\bangke.dll
```

注销一次生效。新版安装包已把 `RegisterTSF` 改成 `Impersonate="yes"`，不会再这样。

**二、在列表里但打了还是英文。** 那是服务端的问题——服务端连不上时 `_ProcessKeyEvent`
把按键原样放行（`*pfEaten = FALSE`）。查三件事：

- `Get-Process BangkeServer` 在不在跑；
- 它跑在哪个 session（`Select-Object SessionId`），**要与你的桌面会话一致**——RDP 下
  特别容易错开；
- 它启动时是不是把自己停用了：`_Initialize` 里 `rime_api->start_maintenance()` 返回真
  （需要首部署）会置 `m_disabled`，要等一次 `EndMaintenance` 才恢复。

日志在 `%TEMP%\rime.bangke`。

## 远程驱动（Mac → 17-Windows）

`ssh bangke@192.168.101.17`（公钥免密）。执行 PS 用 EncodedCommand：

```bash
ENC=$(iconv -f UTF-8 -t UTF-16LE script.ps1 | base64 | tr -d '\n')
ssh bangke@192.168.101.17 "powershell -NoProfile -EncodedCommand $ENC"
```

勿用 `powershell -Command -` + stdin（多行代码块被逐行执行搞坏）、勿裸引号内联（cmd 抢解析管道符）。

## 2026-10-15 Linux main 功能对齐记录

对照范围：`bangke-fcitx5-rime-main` 的 `main`，2026-10-01 起至
`51f9afa`，46 个非合并提交。Windows 修改仅在本私有库进行，保留任务开始时已有未提交改动。

| Linux 提交 / 功能组 | Windows 对应处理 |
|---|---|
| `f754125` AI 自动浮现、过滤标点、模型选择 | 原有 Win32 泵和快照推送保留；新增 150→300→600ms 退避、tick 回绕处理；模型可选择、预检三个必需文件、恢复默认、按方案保存 |
| `89959ae` 拼写提示默认关闭 | 通用设置默认值、共享配置、外观预览统一为关闭；已有显式用户配置保留 |
| `1bf640b` / `0ebeddf` 默认雾凇、品牌方案名 | 原有雾凇优先不变；新用户方案种子加入“蚌壳-雾凇 / 蚌壳-明月” |
| `3a660ad` / `7b7f48c` 更新检查、通知、明确反馈 | 保留任务开始时已有的 Windows 更新监视与关于页接线；补当前/官网版本和“无需下载”反馈；Windows 不需要 Debian 测试版后缀规则 |
| `3245e1c` 安装卫生与用户数据保留 | 修复引导壳目录删除参数、并发解包互删、日志随载荷丢失、重装强制覆盖、进度混用、重启误提示；挂起操作只清本产品删除项，保留文件替换项 |
| `a688a4d` / `f4a0d3a` 插件完整输入、候选守卫、单字母清理 | 本地插件树已是 `5ee31dd`，含这些修复；远端最终构建前须确认其插件版本，不能仅更新前端 |
| `338245f` 等保存/字体修复 | Linux 的 classicui/DBus 重启机制不适用于 TSF；Windows 保留现有轻量样式重载/全量方案部署，补 AI/行为页草稿载入守卫和 AI 原子保存 |
| `4bf69e9` / `920da1f` 候选 UI | GNOME 扩展、Hyprland 面板为 Linux 专属；Windows 保留原生 TSF/DirectWrite 候选窗和任务开始时已有 UI 改动 |
| Arch/Ubuntu 打包、Boost、Lua、Wayland/XWayland、依赖路径与诊断提交 | 不移植 Linux 脚本；Windows 使用私有 librime、CT2 静态库和既有 Lua 铺料检查。本地 librime 相比共享库少的近期提交均为 Linux 工具链或 release 构建选项 |

另外修复了 AI/行为页切换丢草稿、过期注释、开发卸载遗漏 HKLM 自启和更新监视进程、
安装完成重复启动服务、卸载清理硬编码系统目录，以及 PendingFileRenameOperations
解析缺少类型/边界检查。新版安装界面采用石墨黑、细线几何蚌壳和青绿主按钮，
移除旧场景持续重绘；进度条动画只在可见时运行。

### 自动验证

安装策略测试独立于 Qt 和 Windows，直接覆盖生产代码中的版本解析、安装/卸载阶段、
英文/中文日志动作识别、成功/重启返回码；Release 构建仍保留断言。

```sh
cmake -S installer/tests -B /tmp/bangke-installer-tests -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/bangke-installer-tests
ctest --test-dir /tmp/bangke-installer-tests --output-on-failure
```

首轮状态（已由下节 Windows 实机验证补全）：2026-10-15 macOS CTest 1/1 通过；Clang AddressSanitizer/UndefinedBehaviorSanitizer
检查通过。未完成 Windows 全量编译、安装、升级、卸载、重启和实际视觉验收。
本次执行环境连接 `192.168.101.17:22` 返回 `Operation not permitted`，无法同步或运行远端代码。

### 17 机器后续验收

在 `C:\dev\bangke` 私有库中保留并核对远端原有修改后同步本次文件，
先确认 `librime/plugins/ai-predict` 包含 `5ee31dd`，再按上文构建完整版本。
执行 `scripts/preflight.ps1`，并实际检查：

1. 全新安装后可用 Win+Space 切换，默认雾凇，AI 候选无需继续按键即可浮现。
2. 长输入无明显轮询卡顿；切换应用、清空输入、退出后不再推送旧候选。
3. 选择完整/不完整模型目录、恢复默认、切换方案和设置页、保存重启后的模型路径。
4. 外观和行为保存后真实候选窗生效，拼写提示默认关闭，已有配置不被覆盖。
5. 新版升级、同包修复、不同 ProductCode 同版本重打包、旧版包降级禁用。
6. 卸载保留个人词库，其他输入法注册不受影响；卸载后未重启即重装，再重启仍可输入。
7. 安装失败后日志仍存在；关闭按钮在事务期间被阻止；需要重启时提示正确。
8. 更新检查最新/有更新/离线/跳过版本/关闭自动检查；高 DPI 与键盘操作下安装 UI 可用。

## 2026-10-16 Windows 实机验证

已重新获取 Linux `origin/main`，仍为 `51f9afa`。远端私有库与本地基线均为
`44184a4`，原有远端改动已逐文件比较并备份在
`C:\dev\bangke-backup-20261016`，公开库 `C:\dev\weasel` 未修改。
构建、安装版本为 **0.4.49.1424**。

本轮继续修复了实际执行暴露的问题：

- 远端 AI 插件是一个记录 Mac 绝对路径的占位文件；已铺入私有插件 `5ee31dd` 源码。
- 私有插件 `predict_module.cc` 原来按平台推断 glog 独立，在 Windows 单 DLL 链接中
  与主程序共用实例，导致重复 InitGoogleLogging 启动崩溃。改为检测实际初始化状态，
  仅关闭插件自身拥有的日志实例；共享日志不再由插件改写。
- MSI 启动服务错误地调用 `[System64Folder]explorer.exe`，已改为 `[WindowsFolder]explorer.exe`。
- TSF 注册描述长度改为字符数，修复注册表删除路径拼写/根键和未初始化句柄问题；
  旧 IME 查找不再把官方 `weasel.ime` 当作本产品的替代布局。参数契约见
  [微软 RegisterProfile 文档](https://learn.microsoft.com/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-registerprofile)。
- 设置程序使用 Qt 6.8 的汉字 fallback API，避免 QML 单独设置 Segoe UI 后回落到宋体；
  新按钮改用项目既有 Btn，自动更新开关使用既有 ToggleRow。
- 数据构建中的 `%%~nf` 实际产生 `rime_ice.schema`，旧过滤规则会误删全部方案。
  改为按完整文件名过滤；打包前对缓存数据重新应用品牌名和默认样式，并校验方案/模型文件。
- 单文件引导壳只携带 Widgets 运行库，QML/网络模块已在 MSI 中，不再重复嵌入。
  必需载荷缺失直接失败；MSI 不再安装只供引导壳使用的 BangkeInstaller.exe。
- 进度解析覆盖中文“操作开始”和全角句号，忽略升级中旧产品卸载的提前结束标记。
- 首次模型复制中断时，原来仅按目录存在判断会跳过残缺文件；现在补齐缺失/空文件，
  保留非空用户文件，并验证重复执行不会改写它们。
- IPC 调试工具改为解析当前二进制协议，修正虚函数签名，并新增 `/smoke-ai` 实测路径；
  preflight 的二进制扫描改用编译后的循环，避免 PowerShell 逐字节扫描大 DLL。

实际通过的检查：

| 检查 | 结果 |
|---|---|
| Windows x64 librime + 私有 AI 插件 / 原生前端 / Qt 设置 / 安装器 / MSI / 单 EXE | 构建成功 |
| 安装策略与模型文件修复 CTest | 2/2 通过 |
| Qt 更新检查 + 真实 librime AI 配置 CTest | 2/2 通过，配置和更新设置均使用隔离临时目录 |
| 真实 HTTPS 更新接口 | 通过，显示当前与官网版本，不提出降级 |
| 协议编码/截断回归 TestResponseParser | ALL PASS |
| 0.4.48 升级 / 同版本不同 ProductCode 更新 / 修复 / 卸载 / 未重启重装 | 返回 0，用户 `.custom.yaml` 哈希保持一致 |
| 安装后服务 | 在桌面会话 1 持续运行，用户输入法列表包含蚌壳 |
| 真实服务 AI 异步快照 | PASS，输入停止后自动到达，未补按键；测试会话结束时清理自己的 composition |
| 部署 preflight | ALL GREEN（开发工作树显式使用 `-AllowDirty`） |
| 150% DPI 设置页与安装器 | 实际启动并截图，AI/关于页无 QML 加载错误 |

测试命令（Windows 私有库根目录，先按 env.bat 配置 PATH）：

```powershell
$env:PATH = "C:\dev\bangke\output;C:\Libraries\Qt\6.8.3\msvc2022_64\bin;" + $env:PATH
cmake -S BangkeSettings/tests -B BangkeSettings/tests/build -A x64 -DCMAKE_PREFIX_PATH=C:/Libraries/Qt/6.8.3/msvc2022_64
cmake --build BangkeSettings/tests/build --config Release
ctest --test-dir BangkeSettings/tests/build -C Release --output-on-failure
.\BangkeSettings\tests\build\Release\updatechecker_test.exe --live
.\x64\Release\TestResponseParser.exe
# 以下两项须在已启动输入法的交互桌面会话中运行：
.\x64\Release\TestBangkeIPC.exe /smoke-ai
powershell -File scripts/preflight.ps1 -AllowDirty
```

未执行注销或重启；本次实际安装链均返回 0，未要求强制重启。
仍保留 3010/1641 的重启提示分支测试，但未模拟真实文件占用后重启 Windows。
未把本地测试版本发布到网站更新源，源站仍按现有正式发布版本返回。

单文件 EXE 已在桌面会话中实际打开，点击主按钮完成同版本新 ProductCode 更新，
欢迎、安装进度、完成页均已截图；MSI 最终返回 0。引导壳退出后临时载荷目录删除，
独立日志保留；再次通过持续运行、异步 AI 快照及安装目录引擎哈希检查。
安装器策略和模型文件测试在 macOS、Windows 均为 2/2 通过。

本地交付文件位于 `dist/`（Git 忽略），截图和运行记录位于 `dist/verification/`。
源码保留为工作树修改，包含任务开始前已有的修改；未自动提交或发布。

重复运行同一个单文件 EXE 的修复路径也已覆盖：同 ProductCode 使用
`/fvomus <本次包内 MSI>`，刷新 Windows Installer 缓存并从当前包修复；
不同 ProductCode 使用正常 `/i` 升级链。避免对已删除的历史解包目录寻源。
旧包不提供降级或修复入口。系统应用列表隐藏无有效源的原生 Modify/Repair 入口；
修复由分发 EXE 中的界面承担，系统卸载入口保留。

最终 EXE 已连续执行两次，覆盖同版本不同包的更新和同包再次修复，均返回 0；
第二次日志确认 `REINSTALLMODE=vomus`，已删除上次解包目录也能修复。
最后修正了 AI 页“候选位置必须大于扫描范围”的过期说明：位置从 0 计数，扫描范围仅用于去重。
功能对齐阶段包 SHA-256：`fa4fb3bdfaa1e2458ffeb2ca06204de02adcf0209c9388c2978a17e6ab91ff06`。


### 设置页视觉调整（2026-10-16）

设置窗采用随系统变化的浅深色导航栏、工作区和青绿强调色，默认尺寸 960×660，
最小尺寸 780×580。移除口号、页头副标题、重复分组标题和散落的问号，
说明改为悬停提示。外观页以五个分段组织设置，双主题预览固定显示；
智能联想按候选、响应、生成、模型四个分段展示，输入方案保持在顶部。
卡片使用单层细边框，设置文字及预览均使用平台字体绘制。

透明度以 macOS 标准实现为准：先绘制完整候选窗，再通过 ULW 常量 alpha
统一施加不透明度，背景、高亮和文字一起变化。方案本身的颜色 alpha 保留，
不再额外乘背景或分别混色。预览通过 Qt Quick 离屏层统一合成，UI 名称为
“窗口透明度”，配置数值不变。IPC 的 window_alpha 字段沿用现有布局。
整张设置窗保持不透明，避免 Acrylic 叠加灰色染色层。

实机渲染日志确认 Qt Quick 使用 D3D11 和 AMD Radeon 780M Graphics，
未使用 Microsoft Basic Render Driver。窗口透明度测试包含 0、1、中间值及非法值，另用像素检查
验证背景、高亮在 50% 时同步合成、全透明时一起消失；macOS 独立编译通过，Windows 设置测试 4/4 通过。

UI 手工检查覆盖所有主页面、外观五个分段、AI 四个分段、下拉菜单、
背景透明度两端和最小尺寸。100% 与进程级 `QT_SCALE_FACTOR=1.5` 均通过，
未更改系统显示设置；深色设置主题尚未实机截图。
重打包后通过单文件 GUI 安装，MSI 返回 0；安装目录设置程序、服务端与构建
产物哈希一致，安装后异步 AI IPC 验证通过。源码保留为私有库工作树修改。
安装包：`dist/BangkeSetup-0.4.49.1424-win64.exe`，SHA-256：
`3e568d951129d236e864d3be56a2d2b8496c551c65e83f2295e697ec1011fcdd`。
截图：`dist/verification/professional*.png`。

下拉菜单修复：鼠标悬停项和键盘当前项不再分别高亮，统一为单一活动项；
移出、滚动和关闭时清理活动状态，打开时恢复当前值并获得键盘焦点。
Qt Quick 回归测试直接使用生产 ComboRow/Sidebar，覆盖连续移入移出、
鼠标与键盘交替、回车选择及重新打开，以及侧栏深浅主题切换和轻微色差。
侧栏改用 Theme 语义色，浅色背景为 #F2F5F7，深色背景为 #0C0C0C，
均只比工作区略深，并以细线分界。
部署扫描限定为 BangkeSettings/qml，避免测试 QML 的 QtTest 导入进入安装包；
trim_output 同时清除历史输出中的 QtTest 模块和测试运行库。

本轮单文件安装已完成，MSI 返回 0。最新截图为 `dist/verification/menu-sidebar*.png`，
回归结果为 `dist/verification/menu-sidebar-regression.log`。

保存并应用对齐 macOS：没有修改时直接关闭；有修改时等待成功应用后关闭。
写入或应用失败保留窗口和草稿，记录待应用状态，支持没有新增修改时再次重试。
各模型区分“无变化”和“保存失败”，部分写入也保留待部署标记；保存成功后更新
样式与方案快照，避免重复保存。后台应用期间禁止关闭窗口或继续编辑，防止
线程回调访问已销毁的控制器。部署与重载检查返回值，错误码 1 不再静默忽略。
实机已覆盖无修改关闭、修改保存关闭、服务不可用时不关闭并恢复服务后重试关闭，
以及空方案校验失败保留窗口；测试前后原始用户配置字节已恢复并重新部署。
回归记录：`dist/verification/mac-align-save.log`、`mac-align-regression.log`。

构建脚本仅在重建原生前端、引擎或 OpenCC 时停止服务；只构建设置程序不再
调用 BangkeServer /q，避免开发 UI 时无故中断正在使用的输入法。
