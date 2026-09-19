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
`build.bat` 取 `%CD%`），搬过去即可。`output\data`（plum 配方 + 雾凇词库 + opencc）是纯
数据，也可以直接搬，省掉走 GitHub 那一步。

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

## librime-lua 也得单独铺

同一类事：`librime-lua` 同样不在 librime 仓库里，各平台各自铺（mac 上目录名叫 `lua`，
Windows 上叫 `librime-lua`）。**雾凇拼音的日期、农历、数字、计算器等组件全是 lua
translator，少了它 rime.dll 照样编得出来，只是那些组件全部加载失败** —— 症状是那些功能
静默消失，而不是报错。

`build.bat rime` 现在按「`librime\plugins` 下有没有名字带 lua 的目录」挡一道。这台机器上
那份在 `C:\dev\weasel\librime\plugins\librime-lua`（公开树的检出里带着），搬过来即可：

```powershell
robocopy C:\dev\weasel\librime\plugins\librime-lua C:\dev\bangke\librime\plugins\librime-lua /E
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

## 远程驱动（Mac → 17-Windows）

`ssh bangke@192.168.101.17`（公钥免密）。执行 PS 用 EncodedCommand：

```bash
ENC=$(iconv -f UTF-8 -t UTF-16LE script.ps1 | base64 | tr -d '\n')
ssh bangke@192.168.101.17 "powershell -NoProfile -EncodedCommand $ENC"
```

勿用 `powershell -Command -` + stdin（多行代码块被逐行执行搞坏）、勿裸引号内联（cmd 抢解析管道符）。
