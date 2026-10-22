# 蚌壳拼音 · Bangke Pinyin

Windows 平台的 [Rime](https://rime.im) 输入法,基于 [rime/weasel](https://github.com/rime/weasel)(小狼毫)深度定制的 x64-only 分支。

## 我们扩展了什么

- **AI 候选预测**:内置 [bangke-ai-predict](https://github.com/wyjrichhh/bangke-ai-predict) 插件,CTranslate2 本地推理,长句输入时 AI 候选实时浮现(带 `AI` 标记),按键序号直选上屏
- **内置方案**:明月拼音(`luna_pinyin`)与雾凇拼音(`rime_ice`,含词库与 librime-lua),安装即用
- **现代候选面板**:纯 Direct2D 绘制,胶囊高亮、圆角、环阴影、逐像素半透明,跟随系统深浅色;8 套精选配色 + 完全自定义
- **Qt6 / QML 设置程序**:提供外观、输入行为、方案、AI 和关于页面,包含候选预览与更新检查,保存配置后重新部署
- **WiX MSI 安装器**:带 Qt 前端(装/修/卸/升级),全新安装自动播种用户配置与 AI 模型
- **版本化二进制协议**:服务端↔TSF 前端走帧协议,按会话共享内存推送 AI 快照

## 实现机制

### AI 结果如何进入 TSF 前端

`BangkeRime/` 管理 Rime 会话和插件交互，`BangkeServer/` 在服务端处理输入。AI 模型在插件后台线程推理，结果经候选流水线融合，再通过版本化 IPC 帧和按会话共享内存快照送到 TSF 前端。前端消费会话状态并刷新候选，避免将模型推理放进应用的按键处理路径。

`BangkeUI/` 用 Direct2D 绘制候选面板；窗口透明度由合成后的窗口整体控制，颜色透明度与窗口透明度分别处理。

### 设置、模型与安装

`BangkeSettings/` 的 C++ 模型负责读取和保存配置，QML 提供设置页面与预览，更新检查通过独立网络组件完成。前端样式、通用行为和 schema 中的 AI 配置分别写入对应位置。

安装体系由 WiX MSI、安装前端和引导程序组成，处理安装、修复、卸载与升级。模型文件补齐逻辑递归复制缺失或空文件，保留已有非空用户文件，供首次初始化和不完整安装修复使用。

基础 Rime / 小狼毫功能与通用配置请参阅[上游项目](https://github.com/rime/weasel#readme)。

## 构建

见 [docs/win-build.md](docs/win-build.md)(Visual Studio 2022 + boost + Qt6 + WiX,x64-only)。

```bat
build.bat          :: weasel 主件(dll/server/tests)
build.bat settings :: Qt 设置程序
build.bat data     :: 数据(plum + 雾凇离线铺设)
installer\build_msi.bat :: MSI 安装包
```

## 目录

| 目录 | 内容 |
|---|---|
| `BangkeTSF/` | TSF 文本服务(前端,候选窗在应用进程内绘制) |
| `BangkeServer/` | 算法服务进程 |
| `BangkeIPC/` `BangkeIPCServer/` | 管道通信与二进制协议 |
| `BangkeUI/` | Direct2D 候选面板与布局 |
| `BangkeRime/` | Rime 集成层 |
| `BangkeSettings/` | Qt 设置程序 |
| `installer/` | WiX MSI + 安装器前端 |
| `librime/plugins/` | Rime 插件集成，AI 实现见独立插件仓库 |

> 兼容性说明:rime 配置节点沿用了上游的 `weasel.yaml` / `weasel.custom.yaml` 文件名,以保持用户配置习惯与文档通用性。

## 验证入口

- `test/TestBangkeIPC/`：IPC 帧与会话数据回归检查。
- `installer/tests/`：安装策略、路径与模型文件补齐测试，可单独用 CMake / CTest 构建运行。
- `BangkeSettings/tests/`：更新检查、AI 设置模型、透明度和 QML 交互测试；部分测试依赖 Windows 与 Qt6。

## 致谢

- [rime/weasel](https://github.com/rime/weasel) 及其贡献者 —— 本项目的基础
- [rime](https://github.com/rime/librime) 输入法引擎
- [雾凇拼音 rime-ice](https://github.com/iDvel/rime-ice)
- [CTranslate2](https://github.com/OpenNMT/CTranslate2)

## 许可

GPL-3.0,见 [LICENSE.txt](LICENSE.txt)。
