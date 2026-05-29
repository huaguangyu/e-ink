# 从源码构建指南

本文档说明如何从源码编译构建 e-ink 的三个组件。如果你只想使用，不需要编译，请直接看 [README.md](README.md) 从 Release 下载。

---

## 前置环境

| 工具 | 用途 | 安装 |
|------|------|------|
| Go 1.26+ | 编译 quota_go 和 smart_control | https://go.dev/dl/ |
| GCC / CGO | smart_control 依赖 SQLite（CGO） | Linux: `sudo apt install build-essential`，macOS: `xcode-select --install` |
| PlatformIO | 编译烧录 ESP32 固件 | `pip install platformio` 或 VSCode 插件 |
| USB 数据线 | 烧录固件 | 需支持数据传输 |

---

## 一、编译 ESP32-C3 固件

固件基于 PlatformIO + Arduino 框架，适配 ESP32-C3 + 4.2 寸墨水屏。

### 编译

```bash
cd firmware

# 三色屏 (WFT0420CZ15) — 默认
pio run -e wft0420cz15

# 黑白屏 (GDEY042T81)
pio run -e gdey042t81
```

### 固件版本号

构建时会自动注入固件版本号。优先使用当前提交对应的 Git tag：

```
v1.0.7
```

如果当前提交没有对应 tag，则使用 7 位短 SHA：

```
gabcdef0
```

设备信息页的 `FW` 字段和智控台心跳中的 `firmware` 字段都会使用这个版本号。这样拿到设备截图或 Dashboard 状态后，可以直接定位到对应 Release 或源码提交。

如果本地工作区有未提交改动，版本号会显示为本地构建时间：

```
local-20260528-1722
```

构建脚本会先生成 `firmware/src/firmware_version.h`，再编译固件。如果不在 Git 仓库中构建，或构建脚本没有拿到 Git 信息，版本号也会回退为本地构建时间。

如果设备信息页没有显示最新版本号，通常是旧的编译缓存没有清掉。请执行一次清理后重编：

```bash
pio run -e wft0420cz15 -t clean
pio run -e wft0420cz15
```

### 烧录

USB 数据线连接设备和电脑：

```bash
# 编译 + 烧录
pio run -e wft0420cz15 -t upload

# 烧录后查看串口输出（波特率 115200）
pio device monitor
```

> 烧录失败时：按住 BOOT 键 → 点按 EN 键 → 松开 EN → 松开 BOOT → 重新烧录。

### 产物位置

```
firmware/.pio/build/wft0420cz15/firmware_merged.bin
firmware/.pio/build/gdey042t81/firmware_merged.bin
```

`firmware_merged.bin` 是包含 bootloader、分区表和应用的单文件固件，可用 `esptool write_flash 0x0` 直接烧录。`firmware.bin` 只是应用分区镜像，不能作为 Release 单文件固件使用。

### 依赖说明

PlatformIO 会自动安装以下依赖（在 `platformio.ini` 中声明）：

- `bblanchon/ArduinoJson@^7.0` — JSON 解析
- `zinggjm/GxEPD2@^1.6.9` — 墨水屏驱动

### 自定义屏幕

编辑 `firmware/platformio.ini`，修改构建环境中的 `EPD_WIDTH`、`EPD_HEIGHT` 和显示宏定义。只要是 GxEPD2 库支持的 4.2 寸屏幕均可适配。

---

## 二、编译配额服务 (quota_go)

纯 Go 项目，无 CGO 依赖，可轻松交叉编译。

### 本机编译

```bash
cd backend/quota_go
go build -o quota_checker ./src
```

### 交叉编译

```bash
# Linux AMD64
GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o quota_checker_linux_amd64 ./src

# Linux ARM64（树莓派、NAS）
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o quota_checker_linux_arm64 ./src

# macOS Apple Silicon
GOOS=darwin GOARCH=arm64 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o quota_checker_darwin_arm64 ./src

# Windows AMD64
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o quota_checker_windows_amd64.exe ./src
```

### 依赖

```bash
go mod tidy   # 自动处理依赖
```

唯一的第三方依赖：`github.com/easychen/serverchan-sdk-golang`（Server酱推送）。

### 配置

从模板复制配置文件到同目录：

```bash
cp .config.example.json .config.json
```

编辑 `.config.json`，填写 `zhipu.api_key`（必填），其余按需修改：
- `server_path`：HTTP 接口路径，配网时设备填写的地址需与此一致
- `cache_ttl`：缓存时间（秒），默认 600（10 分钟）
- `port`：监听端口，默认 12001

### 运行测试

```bash
# 编译后直接运行
go run ./src

# 指定子命令
go run ./src fetch gemini
go run ./src show codex
go run ./src login codex
```

---

## 三、编译智控台 (smart_control)

Go + CGO 项目（依赖 `go-sqlite3`），交叉编译需要额外注意。

### 本机编译

```bash
cd backend/smart_control
CGO_ENABLED=1 go build -trimpath -ldflags="-s -w" -o elnk-console .
```

### 使用 build.sh 打包

```bash
# 编译当前平台并打包（产物含 static/ 目录）
./build.sh darwin-arm64
./build.sh linux-amd64
./build.sh all

# 产物输出到 dist/
# dist/elnk-console-darwin-arm64.tar.gz
# dist/elnk-console-linux-amd64.tar.gz
```

### 交叉编译 Linux AMD64

由于 CGO 依赖，交叉编译需要对应的 C 交叉编译工具链：

**方式一：有交叉编译器**

```bash
CC=x86_64-linux-gnu-gcc CGO_ENABLED=1 GOOS=linux GOARCH=amd64 \
  go build -trimpath -ldflags="-s -w" -o elnk-console .
```

**方式二：使用 Docker**

```bash
# build.sh 会自动检测 Docker 并使用 golang:latest 镜像编译
./build.sh linux-amd64
```

### 依赖

```bash
go mod tidy
```

主要依赖：
- `github.com/gin-gonic/gin` — HTTP 框架
- `github.com/gorilla/websocket` — WebSocket
- `github.com/mattn/go-sqlite3` — SQLite（**需要 CGO**）

> 国内服务器 `proxy.golang.org` 访问慢时：`go env -w GOPROXY=https://goproxy.cn,direct`

---

## 四、验证构建结果

| 组件 | 验证命令 |
|------|---------|
| 固件 | `pio device monitor` 查看启动日志 |
| quota_go | `./quota_checker fetch` 拉取配额 |
| smart_control | `./elnk-console server start && curl localhost:9857/health` |

---

## 五、CI/CD 自动构建

项目配置了 GitHub Actions（`.github/workflows/release.yml`），推送 `v*` tag 时自动编译：

- firmware: 两种屏幕型号的 `.bin` 固件
- quota_checker: linux/amd64, linux/arm64, darwin/arm64, windows/amd64
- smart_control: linux-amd64, darwin-arm64（含 static/ 目录）

触发：

```bash
git tag v1.0.0
git push origin v1.0.0
```

构建产物自动附加到 GitHub Release。
