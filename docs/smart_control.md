# smart_control - eink 智控台

`smart_control` 是 Go 实现的设备智控台。它负责接收 ESP32 心跳、保存心跳历史、管理远程任务、通过 WebSocket 推送 Dashboard 更新，并代理展示配额服务数据。

## 模块职责

- 提供 Dashboard 页面：`static/index.html`
- 接收设备心跳：电压、电量、IP、唤醒原因、RSSI、heap、固件版本、当前唤醒间隔
- 管理任务队列：修改唤醒间隔、重启、切换显示页面、触发对时
- 提供 WebSocket，Dashboard 收到设备心跳或任务变化时实时更新
- 使用 SQLite 持久化心跳和任务
- 代理 `quota_go` 配额接口给浏览器 Dashboard 展示
- 支持后台运行命令：`server start|stop|restart|status`

默认监听：

```text
0.0.0.0:9857
```

生产访问方式建议：

```text
浏览器 / ESP32 → 域名 http://your-console.example.com → nginx → 127.0.0.1:9857
```

## 目录结构

```
backend/smart_control/
├── main.go              # HTTP 路由和 handler
├── config.go            # 环境变量配置
├── db.go                # SQLite schema 和数据访问
├── daemon.go            # 后台服务 start/stop/restart/status
├── models.go            # 请求、响应、DB 数据结构
├── schedule.go          # 动态睡眠调度算法、验证、API handler
├── ws.go                # WebSocket Hub
├── build.sh             # darwin-arm64 / linux-amd64 打包脚本
├── static/
│   └── index.html       # Dashboard 页面
├── go.mod
└── README.md
```

运行时文件：

```text
eink_console.db          # SQLite 数据库
eink_console.db-shm      # WAL 运行时文件
eink_console.db-wal      # WAL 运行时文件
run/app.pid              # 后台服务 PID
```

## 配置

通过环境变量配置：

| 变量 | 默认值 | 说明 |
|---|---|---|
| `EINK_CONSOLE_ADDR` | `0.0.0.0:9857` | HTTP 监听地址 |
| `EINK_CONSOLE_DB` | `eink_console.db` | SQLite 数据库路径 |
| `EINK_QUOTA_URL` | `http://YOUR_SERVER_IP:12001/LimiT/quErY` | 配额服务地址 |
| `EINK_HTTP_TIMEOUT_SEC` | `8` | 代理配额请求超时秒数 |
| `EINK_KEEP_HEARTBEATS` | `1000` | 每台设备保留最近心跳数量 |

## 本地构建

`smart_control` 使用 SQLite，依赖 `github.com/mattn/go-sqlite3`，所以本地构建需要启用 CGO。

从仓库根目录构建：

```bash
cd backend/smart_control
CGO_ENABLED=1 go build -trimpath -ldflags="-s -w" -o elnk-console .
```

也可以在仓库根目录一行构建：

```bash
(cd backend/smart_control && CGO_ENABLED=1 go build -trimpath -ldflags="-s -w" -o elnk-console .)
```

macOS 如果提示找不到 C 编译器，先安装 Xcode Command Line Tools：

```bash
xcode-select --install
```

构建完成后，当前目录会生成 `elnk-console` 可执行文件：

```bash
./elnk-console server start
curl http://localhost:9857/health
```

如果需要同时打包可执行文件和 `static/` 前端资源，使用项目内脚本：

```bash
cd backend/smart_control
./build.sh darwin-arm64
./build.sh linux-amd64
./build.sh all
```

产物输出到 `backend/smart_control/dist/`。

## 数据库

SQLite 使用 WAL 模式，`SetMaxOpenConns(1)`，避免 SQLite 并发写锁问题。

### sleep_schedules

每台设备一条记录，存储动态睡眠调度配置。

```sql
CREATE TABLE sleep_schedules (
    mac         TEXT PRIMARY KEY,
    mode        TEXT    NOT NULL DEFAULT 'fixed',
    fixed_min   INTEGER NOT NULL DEFAULT 5,
    default_min INTEGER NOT NULL DEFAULT 30,
    slots       TEXT    NOT NULL DEFAULT '[]',
    updated_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

| 字段 | 含义 |
|---|---|
| `mac` | 设备 MAC（主键） |
| `mode` | `fixed` 固定间隔 / `custom` 自定义时段 |
| `fixed_min` | 固定模式的唤醒间隔（分钟） |
| `default_min` | 自定义模式下无时段时的备用间隔 |
| `slots` | JSON 数组，时段配置，见下方 |
| `updated_at` | 最后更新时间 |

slots 格式：

```json
[
  {"start": "06:00", "end": "07:00", "interval_min": 5},
  {"start": "08:00", "end": "18:00", "interval_min": 10}
]
```

- 支持 `end <= start` 表示跨午夜（如 `"22:00"` → `"06:00"`）
- 时段不可重叠，最多 24 条

### heartbeats

每次设备唤醒上报一条心跳。

```sql
CREATE TABLE heartbeats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac TEXT NOT NULL,
    voltage REAL,
    pct INTEGER,
    ip TEXT,
    wake TEXT,
    uptime_s INTEGER,
    device_ts TEXT,
    free_heap INTEGER,
    rssi INTEGER,
    ssid TEXT,
    firmware TEXT,
    sleep_min INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

字段说明：

| 字段 | 含义 |
|---|---|
| `mac` | 设备 MAC，固件上报时去掉冒号 |
| `voltage` | 电池真实电压 |
| `pct` | 电量百分比 |
| `ip` | 设备局域网 IP |
| `wake` | 唤醒原因：`boot` / `timer` / `button` |
| `uptime_s` | 本次启动运行秒数 |
| `device_ts` | 设备本地时间 |
| `free_heap` | ESP32 剩余 heap |
| `rssi` | WiFi 信号强度 |
| `ssid` | 当前连接的 WiFi 名称 |
| `firmware` | 固件构建版本，默认格式为 `g` + Git 短 SHA |
| `sleep_min` | 设备当前 NVS 中的唤醒间隔 |
| `created_at` | 服务端收到心跳时间 |

清理策略：

- 每次写入心跳后，按 MAC 删除旧记录。
- 每台设备只保留最近 `EINK_KEEP_HEARTBEATS` 条，默认 1000 条。

### tasks

Dashboard 创建的远程任务。

```sql
CREATE TABLE tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mac TEXT NOT NULL,
    type TEXT NOT NULL,
    params TEXT,
    sort_order INTEGER DEFAULT 0,
    status TEXT DEFAULT 'pending',
    result TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    done_at DATETIME
);
```

任务类型：

| type | params | 设备动作 |
|---|---|---|
| `set_sleep` | `{"minutes":10}` | 更新设备 NVS 中的唤醒间隔 |
| `sync_time` | `{}` | 设备通过 NTP（ntp.aliyun.com / pool.ntp.org）校准本地时间 |
| `restart` | `{}` | 设备上报结果后 `ESP.restart()` |
| `switch_page` | `{"page":0}` | 切换设备显示页面，见下方页面编号表 |

switch_page 页面编号：

| page | 页面 |
|---|---|
| 0 | 总览 |
| 1 | Google |
| 2 | Codex |
| 3 | 智谱 |
| 4 | 设备信息 |

设备收到任务后更新内存中的 `currentPage` 变量，在后续显示刷新时生效。设备上报执行结果后任务标记为 `done`。

任务状态：

```text
pending → sent → done
                → failed
```

| 状态 | 含义 |
|---|---|
| `pending` | 等待设备拉取 |
| `sent` | 已被设备拉取，不再重复下发 |
| `done` | 设备执行成功 |
| `failed` | 设备执行失败 |

去重规则：

- 同一设备同一类型只能有一条 `pending` 任务。
- 重复创建同类型 pending 任务时，更新原任务的 `params`，不新增记录。
- `sent` / `done` / `failed` 不参与 pending 去重。

排序规则：

- Dashboard 拖拽排序写入 `sort_order`。
- 设备拉取任务时按 `sort_order ASC, id ASC` 返回。
- 只有 `pending` 任务可以排序。

送达规则：

- 设备请求 `GET /api/tasks?mac=...` 时，服务端在同一个事务里查询 pending 并标记为 `sent`。
- 这样可以避免重启任务被重复拉取导致循环重启。

删除规则：

- 当前后端 `DELETE /api/tasks/{id}` 会删除任意状态任务记录。
- 如果任务已经被设备拉取成 `sent`，删除数据库记录不能撤回设备本地已拿到的任务。

## 设备 API

### POST /api/heartbeat

设备每次唤醒后上报。

请求：

```json
{
  "mac": "A1B2C3D4E5F6",
  "voltage": 4.10,
  "pct": 95,
  "ip": "192.0.2.10",
  "wake": "timer",
  "uptime_s": 4,
  "device_ts": "2026-05-20 09:23:40",
  "free_heap": 84000,
  "rssi": -45,
  "ssid": "ExampleWiFi",
  "firmware": "g1a2b3c4",
  "sleep_min": 5
}
```

响应：

```json
{
  "sleep_min": 5,
  "persist": true,
  "restart": false
}
```

说明：

- 请求体 `sleep_min` 是设备当前正在使用的唤醒间隔。
- 响应体 `sleep_min` 是服务端根据调度计算的下一次休眠间隔。
- `persist` 为 `true` 时设备写入 NVS Flash（固定模式），为 `false` 时仅写入 RTC 内存（自定义模式，避免 Flash 磨损）。
- 当前 `restart` 固定返回 `false`，重启通过任务队列执行。
- 收到心跳后服务端会通过 WebSocket 广播 `heartbeat`。

### GET /api/tasks

设备拉取任务：

```http
GET /api/tasks?mac=A1B2C3D4E5F6
```

响应：

```json
{
  "tasks": [
    {"id":1,"type":"set_sleep","params":{"minutes":10},"sort_order":0,"status":"sent"},
    {"id":2,"type":"sync_time","params":{},"sort_order":1,"status":"sent"}
  ]
}
```

无任务：

```json
{"tasks":[]}
```

Dashboard 查询历史：

```http
GET /api/tasks?mac=A1B2C3D4E5F6&history=1
GET /api/tasks?mac=A1B2C3D4E5F6&status=pending
```

### POST /api/tasks/{id}/result

设备执行任务后上报结果：

```json
{
  "success": true,
  "data": {}
}
```

响应：

```json
{"ok":true}
```

如果任务已经完成或不存在：

```json
{"error":"task not found or already finished"}
```

## Dashboard API

| 方法 | 路径 | 用途 |
|---|---|---|
| `GET` | `/` | 返回 `static/index.html` |
| `GET` | `/health` | 健康检查 |
| `GET` | `/ws` | WebSocket |
| `GET` | `/api/status?mac=...&limit=20` | 最新心跳、心跳历史、任务列表、设备列表、调度配置 |
| `GET` | `/api/devices` | 所有已知设备列表 |
| `GET` | `/api/schedule?mac=...` | 获取设备调度配置 |
| `PUT` | `/api/schedule` | 更新设备调度配置 |
| `GET` | `/api/quota` | 代理 `quota_go` 全部 provider |
| `GET` | `/api/quota?provider=codex` | 代理单 provider |
| `POST` | `/api/tasks` | 创建或更新 pending 任务 |
| `PUT` | `/api/tasks/sort` | 更新 pending 任务排序 |
| `DELETE` | `/api/tasks/{id}` | 删除任务记录 |

创建任务：

```json
{
  "mac": "A1B2C3D4E5F6",
  "type": "set_sleep",
  "params": {"minutes": 10}
}
```

切换页面任务：

```json
{
  "mac": "A1B2C3D4E5F6",
  "type": "switch_page",
  "params": {"page": 2}
}
```

排序：

```json
{
  "orders": [
    {"id": 1, "sort_order": 0},
    {"id": 2, "sort_order": 1}
  ]
}
```

## WebSocket

Dashboard 连接：

```text
ws://<domain>/ws
```

事件格式：

```json
{
  "type": "heartbeat",
  "data": {}
}
```

当前会广播：

| type | 触发 |
|---|---|
| `heartbeat` | 新心跳写入 |
| `task_changed` | 创建或更新任务 |
| `tasks_claimed` | 设备拉取任务并标记 sent |
| `task_result` | 设备上报任务结果 |
| `task_deleted` | 删除任务 |
| `tasks_sorted` | Dashboard 排序 |
| `schedule_changed` | 调度配置更新 |

## 调度 API

### GET /api/schedule?mac=...

获取设备调度配置。无配置时返回 `{"schedule": null}`。

### PUT /api/schedule

创建或更新调度配置：

```json
{
  "mac": "A1B2C3D4E5F6",
  "mode": "custom",
  "fixed_min": 5,
  "default_min": 30,
  "slots": [
    {"start": "06:00", "end": "07:00", "interval_min": 5},
    {"start": "08:00", "end": "18:00", "interval_min": 10}
  ]
}
```

验证规则：

- `mode` 必须是 `fixed` 或 `custom`
- `fixed_min` / `default_min` 范围 1-1440
- 时段格式 `HH:MM`，`start` 和 `end` 不能相同
- 支持 `end <= start` 表示跨午夜（如 `22:00` → `06:00`）
- 时段不可重叠，最多 24 条
- `interval_min` 范围 1-1440

### 调度计算逻辑

每次设备心跳时，服务端根据调度配置计算 `sleep_min`：

1. **固定模式**：直接返回 `fixed_min`
2. **自定义模式**：
   - 当前时间在某时段内 → 返回该时段的 `interval_min`
   - 不在任何时段 → 返回距离下一个时段开始的分钟数
   - 今天无更多时段 → 跨午夜到明天第一个时段
   - 无任何时段 → 返回 `default_min`

### 固件存储策略

| | 固定模式 | 自定义模式 |
|---|---|---|
| 心跳响应 `persist` | `true` | `false` |
| 存储 | NVS Flash（跨断电持久） | RTC 内存（跨 deep sleep，断电丢失） |
| 恢复 | NVS 直接读取 | 冷启动从 NVS 读默认值，第一次心跳后刷新 |

## 运行

前台运行：

```bash
go run .
```

编译：

```bash
go build -o elnk-console .
```

后台服务：

```bash
./elnk-console server start
./elnk-console server status
./elnk-console server stop
./elnk-console server restart
```

服务会在工作目录或可执行文件目录下寻找 `static/index.html`。后台 PID 写入：

```text
run/app.pid
```

## 打包

```bash
./build.sh darwin-arm64
./build.sh linux-amd64
./build.sh all
```

输出：

| 目标 | 包 |
|---|---|
| macOS Apple Silicon | `dist/elnk-console-darwin-arm64.tar.gz` |
| Linux amd64 | `dist/elnk-console-linux-amd64.tar.gz` |

说明：

- 项目使用 `github.com/mattn/go-sqlite3`，需要 CGO。
- macOS Apple Silicon 可以本机编译 `darwin-arm64`。
- macOS 交叉编译 Linux amd64 需要 `x86_64-linux-gnu-gcc` 或 Docker。
- 服务器上无法访问 `proxy.golang.org` 时，建议设置：

```bash
go env -w GOPROXY=https://goproxy.cn,direct
```

## nginx 部署

服务本机监听 `127.0.0.1:9857` 或 `0.0.0.0:9857`，建议由 nginx 通过域名访问：

```nginx
server {
    listen 80;
    server_name your-console.example.com;

    location / {
        proxy_pass http://127.0.0.1:9857;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location /ws {
        proxy_pass http://127.0.0.1:9857;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }
}
```

部署示例：

```bash
sudo mkdir -p /opt/elnk-console
tar -xzf dist/elnk-console-linux-amd64.tar.gz -C /opt/elnk-console
cd /opt/elnk-console
GIN_MODE=release ./elnk-console server start
```

## 与其他模块的关系

- `firmware` 访问本服务的 `/api/heartbeat`、`/api/tasks`、`/api/tasks/{id}/result`。时间同步通过 NTP 完成，不依赖本服务。
- `quota_go` 是配额数据源；本服务只在 Dashboard 展示时使用。
- 设备配额显示不依赖本服务转发，设备会直连 `quota_go`。
