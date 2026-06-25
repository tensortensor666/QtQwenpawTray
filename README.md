# QwenPawTray

QwenPawTray 是一个 Windows 系统托盘工具，用 Qt/C++ 实现，用来管理本机的 `qwenpaw app` 服务。

## 功能

- 常驻 Windows 系统托盘
- 左键点击托盘图标打开 `http://127.0.0.1:8088/chat`
- 通过托盘菜单启动、停止、重启 `qwenpaw app`
- 启动后自动等待服务就绪，并通过托盘通知提示结果
- 支持为服务和更新流程配置 HTTP/HTTPS 代理
- 支持开机自启，写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- 托盘日志和服务日志写入 `%LOCALAPPDATA%\QwenPawTray\logs`
- 自动清理 7 天前的日志
- 同一 Windows 会话内只允许运行一个托盘实例
- 支持通过 `python -m pip install --upgrade qwenpaw` 更新 QwenPaw

## 环境要求

- Windows x64
- Qt 5 或 Qt 6，需要 `Widgets` 和 `Network` 模块
- CMake 3.16 或更高版本
- MSVC/Visual Studio Build Tools
- Python，并且 `qwenpaw` 可以从 `PATH` 中直接调用
- Inno Setup 6，仅在构建安装包时需要

运行前建议先确认 `qwenpaw` 可用：

```powershell
qwenpaw --version
qwenpaw app
```

## 构建

使用 CMake 配置并构建：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

可执行文件生成位置：

```text
build\Release\QwenPawTray.exe
```

也可以使用项目内的构建脚本：

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
.\scripts\build_release.bat
```

可选环境变量：

- `QT_DIR`：Qt MSVC 套件目录
- `CMAKE_EXE`：CMake 可执行文件，默认使用 `cmake`
- `VSDEVCMD`：`VsDevCmd.bat` 路径

## 构建安装包

执行：

```powershell
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"
.\scripts\build_installer.bat
```

安装包生成位置：

```text
dist\QwenPawTray-Setup.exe
```

如果 Inno Setup 没有安装在默认位置，可以手动指定：

```powershell
$env:ISCC_EXE = "C:\Path\To\ISCC.exe"
```

## 运行时文件

首次运行会自动创建：

```text
%LOCALAPPDATA%\QwenPawTray\tray_config.json
%LOCALAPPDATA%\QwenPawTray\logs\
```

代理配置会立即保存。如果服务已经在运行，需要从托盘菜单重启服务，新的代理环境变量才会生效。

## 上传到 GitHub

构建产物、安装包和部署目录已通过 `.gitignore` 忽略。

新仓库首次提交示例：

```powershell
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin git@github.com:<user>/<repo>.git
git push -u origin main
```
