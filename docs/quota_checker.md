# quota_go - 大模型配额查询服务

`quota_go` 是一个独立 Go 服务，用来查询 Gemini、Codex、智谱 GLM 的配额，并通过 HTTP API 提供给 ESP32 固件和智控台 Dashboard。

## 模块职责

- 查询 Gemini / Claude、Codex、Zhipu GLM 配额。
- 刷新 OAuth token 或 API key 请求数据。
- 统一输出轻量 JSON，便于 ESP32 解析。
- 提供内存缓存 + 文件缓存，减少频繁请求目标网站。
- 支持后台服务命令：`server start|stop|restart|status`。

## 快速开始

### 1. 复制配置文件

```bash
cp .config.example.json .config.json
```

### 2. 编辑 `.config.json`

只需要修改以下字段，其余保持默认即可：

```json
{
  "zhipu": {
    "api_key": "在此填入您的智谱 API Key"
  },
  "notify": {
    "sendkey": "可选，填写 ServerChan sendkey 后失败会推送通知"
  }
}
```

- `gemini` 和 `codex` 的 `client_id`、`client_secret`、`ua` 是固定值，**无需修改**。
- `zhipu.api_key` 是智谱平台分配的永久 API Key，**必填**。
- `notify.sendkey` 为空则不推送，填入 [Server酱3](https://sc3.ft07.com/) 的 sendkey 后查询失败会推送消息。

### 3. OAuth 登录（Gemini / Codex）

首次使用 Gemini 或 Codex 前需要完成 OAuth 登录，获取 refresh_token：

```bash
./quota_checker login gemini    # 按提示完成 Google OAuth 登录
./quota_checker login codex     # 按提示完成 OpenAI OAuth 登录
```

登录成功后凭据会自动保存到 `auths/` 目录。智谱无需登录，只需在 `.config.json` 中填写 `api_key`。

### 4. 启动服务

```bash
./quota_checker server start
```

## 本地构建

`quota_go` 是纯 Go 项目，不依赖 CGO。从仓库根目录构建：

```bash
cd backend/quota_go
go build -o quota_checker ./src
```

也可以在仓库根目录一行构建：

```bash
(cd backend/quota_go && go build -o quota_checker ./src)
```

构建完成后，当前目录会生成 `quota_checker` 可执行文件。运行前复制配置模板：

```bash
cp .config.example.json .config.json
```

### 首次运行自动初始化

如果启动时当前目录没有 `.config.json`，服务会自动：

1. 创建 `.config.json` 模板（需编辑后重启）。
2. 创建 `auths/` 目录。
3. 在 `auths/` 下生成 `gemini_account.json` 和 `codex_account.json` 模板。

## 目录结构

```
backend/quota_go/
├── src/
│   ├── main.go        # CLI 入口
│   ├── config.go      # .config.json、auths、cache 路径
│   ├── provider.go    # provider 注册和分发
│   ├── gemini.go      # Gemini / Claude 配额查询
│   ├── codex.go       # Codex 配额查询
│   ├── zhipu.go       # 智谱 GLM 配额查询
│   ├── server.go      # HTTP 服务、缓存、通知
│   └── daemon.go      # 后台服务 start/stop/status
├── auths/
│   ├── gemini_account.json   # Google OAuth 凭据（login 命令自动生成）
│   └── codex_account.json    # OpenAI OAuth 凭据（login 命令自动生成）
├── cache/             # 运行时缓存，服务自动读写
├── build/             # 构建输出
├── .config.json       # 运行时配置（需从 .config.example.json 复制）
├── .config.example.json  # 配置模板
├── go.mod
└── README.md
```

## 配置文件详解

### `.config.json` 字段说明

| 字段 | 默认值 | 说明 |
|---|---|---|
| `port` | `12001` | HTTP 监听端口 |
| `server_path` | `/LimiT/quErY` | HTTP 接口路径，可自定义 |
| `cache_ttl` | `600` | 缓存有效期，单位秒 |
| `disabled` | `[]` | 禁用的 provider，例如 `["gemini"]` |
| `gemini.client_id` | 固定值 | Google OAuth 应用 ID，无需修改 |
| `gemini.client_secret` | 固定值 | Google OAuth 应用密钥，无需修改 |
| `gemini.oauth_ua` | 固定值 | OAuth token 刷新时的 User-Agent，无需修改 |
| `gemini.api_ua` | 固定值 | 调用 Gemini API 时的 User-Agent，无需修改 |
| `codex.client_id` | 固定值 | OpenAI OAuth 应用 ID，无需修改 |
| `codex.ua` | 固定值 | Codex 请求的 User-Agent，无需修改 |
| `zhipu.api_key` | 无 | 智谱 API Key，**必填**，永久有效 |
| `notify.sendkey` | 空 | [Server酱3](https://sc3.ft07.com/) sendkey，为空则不推送 |
| `accounts` | 默认路径 | 各 provider 账号文件路径，一般无需修改 |

## 账号文件

### Gemini

`auths/gemini_account.json`（由 `login gemini` 命令自动生成）

```json
{
  "refresh_token": "1//...",
  "email": "user@example.com",
  "access_token": "",
  "expired": "",
  "project_id": "your-project-id"
}
```

流程：

1. 检查 `expired`，未过期直接使用 `access_token`。
2. 过期后调用 Google OAuth token 接口刷新。
3. 写回 `access_token` 和 `expired`。
4. 查询 Gemini / Claude 配额；401 时强制刷新后重试一次。

### Codex

`auths/codex_account.json`（由 `login codex` 命令自动生成）

```json
{
  "refresh_token": "rt_...",
  "email": "user@example.com",
  "access_token": "",
  "id_token": "",
  "account_id": "uuid",
  "expired": "",
  "plan_type": "plus"
}
```

流程：

1. 检查 `expired`，未过期直接使用 token。
2. 过期后调用 `https://auth.openai.com/oauth/token` 刷新。
3. 写回 `access_token`、`id_token`、`refresh_token` 和 `expired`。
4. 请求 `https://chatgpt.com/backend-api/wham/usage`。
5. 提取 5 小时窗口和每周窗口配额。

### Zhipu

配置在 `.config.json` 的 `zhipu.api_key` 字段中，不再使用单独的账号文件。

流程：

1. 直接用 API Key 请求智谱监控接口。
2. 提取 TOKENS_LIMIT 的 5 小时 / 每周额度。
3. 提取 TIME_LIMIT 的 MCP 月度额度。

## 缓存机制

请求进入后按顺序命中：

```text
内存缓存未过期
  → 直接返回

文件缓存 cache/{provider}.json 未过期
  → 加载、提取、写入内存缓存、返回

缓存过期或不存在
  → 请求远端平台
  → 提取统一字段
  → 写入内存缓存和文件缓存
  → 返回
```

`cache_ttl` 默认 600 秒。CLI `fetch` 和 HTTP 服务共享 `cache/` 文件。

## HTTP API

### 获取全部 provider

```http
GET /LimiT/quErY
```

返回数组，自动跳过禁用或未配置账号的 provider：

```json
[
  {"p":"gemini","gm":100,"gmr":"21:55","cl":100,"clr":"21:55","t":1779180905},
  {"p":"codex","h5":99,"h5r":"21:50","w":89,"wr":"05-25","t":1779180642},
  {"p":"zhipu","h5":46,"h5r":"19:51","w":60,"wr":"05-21","mcp":99,"mcpr":"06-02","t":1779180906}
]
```

单个平台失败时不影响其他平台，失败项包含 `error`：

```json
[
  {"p":"gemini","error":"refresh_token 已失效"},
  {"p":"codex","h5":99,"h5r":"21:50","w":89,"wr":"05-25","t":1779180642}
]
```

### 获取单个 provider

```http
GET /LimiT/quErY?provider=gemini
GET /LimiT/quErY?provider=codex
GET /LimiT/quErY?provider=zhipu
```

无效 provider：

```json
{"error":"invalid provider","providers":["gemini","codex","zhipu"]}
```

禁用 provider：

```json
{"error":"gemini is disabled"}
```

## 返回字段

| 字段 | 含义 | provider |
|---|---|---|
| `p` | provider 名称 | 全部 |
| `t` | 数据获取时间，epoch 秒 | 全部 |
| `gm` / `gmr` | Gemini 剩余百分比 / 重置时间 | gemini |
| `cl` / `clr` | Claude 剩余百分比 / 重置时间 | gemini |
| `h5` / `h5r` | 5 小时窗口剩余百分比 / 重置时间 | codex, zhipu |
| `w` / `wr` | 每周窗口剩余百分比 / 重置时间 | codex, zhipu |
| `mcp` / `mcpr` | MCP 月度剩余百分比 / 重置时间 | zhipu |
| `error` | 查询失败原因 | 失败 provider |

时间格式由 `fmtReset()` 计算：

| 条件 | 格式 | 示例 |
|---|---|---|
| 重置时间是当天 | `HH:MM` | `21:55` |
| 重置时间不是当天 | `MM-DD` | `05-25` |

## CLI 和后台服务

```bash
go mod tidy
go build -o quota_checker ./src

./quota_checker                         # 获取并展示全部
./quota_checker fetch gemini            # 刷新单个平台缓存
./quota_checker show codex              # 展示缓存
./quota_checker login codex             # Codex (OpenAI) OAuth 登录
./quota_checker login gemini            # Gemini OAuth 登录
./quota_checker server start            # 后台启动 HTTP 服务
./quota_checker server stop
./quota_checker server restart
./quota_checker server status
```

服务监听：

```text
0.0.0.0:{port}
```

## 编译

本机：

```bash
go build -o quota_checker ./src
```

交叉编译示例：

```bash
GOOS=darwin GOARCH=arm64 go build -o build/quota_checker-darwin-arm64 ./src
GOOS=linux GOARCH=amd64 go build -o build/quota_checker-linux-amd64 ./src
```

## 与其他模块的关系

- `firmware` 直连 `GET /LimiT/quErY`，每次唤醒主动获取最新配额。
- `smart_control` 的 `/api/quota` 只为 Dashboard 代理展示，不参与设备任务队列。
