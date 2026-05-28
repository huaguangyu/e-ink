# smart_control - 设备智控台

Go 实现的设备管理控制台，接收 ESP32 心跳、管理远程任务、WebSocket 推送 Dashboard 更新。

## 快速开始

```bash
go run .
# 或编译后
./elnk-console server start
```

默认监听 `0.0.0.0:9857`。

## 配置（环境变量）

| 变量 | 默认值 | 说明 |
|---|---|---|
| `EINK_CONSOLE_ADDR` | `0.0.0.0:9857` | HTTP 监听地址 |
| `EINK_QUOTA_URL` | `http://YOUR_SERVER_IP:12001/LimiT/quErY` | 配额服务地址 |
| `EINK_HTTP_TIMEOUT_SEC` | `8` | 代理超时秒数 |

## 打包

```bash
./build.sh all   # darwin-arm64 + linux-amd64
```

> 详细文档见 [docs/smart_control.md](../../docs/smart_control.md)
