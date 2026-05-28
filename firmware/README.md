# firmware - ESP32-C3 墨水屏固件

基于 PlatformIO + Arduino 框架，适配 ESP32-C3 + 4.2 寸墨水屏。

## 快速开始

```bash
pip install platformio
cd firmware
pio run -e wft0420cz15          # 编译
pio run -e wft0420cz15 -t upload  # 烧录
```

## 支持的屏幕

| 环境名 | 屏幕 |
|---|---|
| `wft0420cz15` | Waveshare 4.2" 三色 400x300 |
| `gdey042t81` | Waveshare 4.2" T81 |

## 按键操作

| 操作 | 效果 |
|---|---|
| 短按 | 切换页面 |
| 长按 2 秒 | 手动刷新当前页 |
| 长按 8 秒 | 进入配网模式 |
| 开机按住 | 强制配网 |

## 工具脚本

- `tools/convert_cn_font.py` — 中文字体转换
- `tools/convert_inter_font.py` — Inter 字体转换
- `merge_firmware.py` — 合并固件

> 详细文档见 [docs/firmware.md](../docs/firmware.md)
