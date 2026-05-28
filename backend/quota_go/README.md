# quota_checker - 大模型配额查询服务

查询 Gemini / Codex / 智谱的大模型配额，通过 HTTP API 提供给 ESP32 和 Dashboard。

## 快速开始

```bash
cp .config.example.json .config.json
# 编辑 .config.json，填写 zhipu.api_key（必填）
./quota_checker login gemini    # Google OAuth 登录
./quota_checker login codex     # OpenAI OAuth 登录
./quota_checker server start
```

验证：`curl http://localhost:12001/LimiT/quErY`

## CLI 命令

```bash
./quota_checker                         # 获取全部配额
./quota_checker fetch gemini            # 刷新单个平台
./quota_checker show codex              # 展示缓存
./quota_checker login <codex|gemini>    # OAuth 登录
./quota_checker server start|stop|restart|status
```

> 详细文档见 [docs/quota_checker.md](../../docs/quota_checker.md)
