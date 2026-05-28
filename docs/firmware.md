# eink - ESP32-C3 电子墨水屏配额看板

基于 ESP32-C3 驱动 4.2 寸电子墨水屏的低功耗看板项目。支持本地绘制 UI、WiFi 配网、深睡眠定时唤醒、电池电压监测。

## 硬件平台

| 项目 | 规格 |
|---|---|
| 主控 | ESP32-C3-WROOM-02 |
| 架构 | RISC-V 160MHz |
| Flash | 4MB |
| Wi-Fi | 2.4GHz |
| 屏幕 | 4.2 寸电子纸 (400x300) |
| 电池 | 锂电池 + ADC 电压检测 |

## 引脚定义

```cpp
// EPD 墨水屏 (SPI)
#define PIN_EPD_MOSI   6    // SPI 数据输出
#define PIN_EPD_SCK    4    // SPI 时钟
#define PIN_EPD_CS     7    // 片选 (低有效)
#define PIN_EPD_DC     1    // 命令/数据选择
#define PIN_EPD_RST    2    // 屏幕复位
#define PIN_EPD_BUSY   10   // 屏幕忙状态

// 板载功能
#define PIN_BAT_ADC    0    // 电池电压 ADC (R1=10k, R2=10k 分压)
#define PIN_CFG_BTN    3    // 用户按键 (INPUT_PULLUP, 低电平按下)
#define PIN_LED        5    // LED 指示灯 (HIGH=亮, LOW=灭)
```

| 按键 | GPIO | 作用 |
|---|---|---|
| EN (RST) | - | 硬件复位，不可编程读取 |
| BOOT / CFG | GPIO 3 | 用户交互按键（固件读取） |

> 注意：BOOT 键同时关联 GPIO 9（用于进入下载模式）。固件中用户按键读 GPIO 3，下载模式由 GPIO 9 负责。按住 BOOT + 点按 EN 可进入下载模式。

## 支持的屏幕

通过 PlatformIO 编译环境切换，生成不同固件：

| 环境 | 屏幕 | 驱动 IC | 颜色 | 状态 |
|---|---|---|---|---|
| `wft0420cz15` | WFT0420CZ15 / GDEW042Z15 类 | UC8176 / IL0398 | 红/白/黑 | GxEPD2 已实现 |
| `gdey042t81` | GDEY042T81 | SSD1683 | 黑白 | GxEPD2 已实现 |

SPI 接口没有 MISO/DOUT 读回线，无法在运行时自动识别屏幕型号，因此采用编译时指定方案。

屏幕驱动和 UI 绘制使用 `zinggjm/GxEPD2@^1.6.9` + Adafruit_GFX，相关说明集中在本文档“显示系统”章节。

## 编译与烧录

### 环境要求

- PlatformIO (CLI 或 VSCode 插件)
- USB 数据线

### 编译

```bash
# 编译三色屏固件 (默认)
~/.platformio/penv/bin/pio run -e wft0420cz15

# 编译黑白屏固件
~/.platformio/penv/bin/pio run -e gdey042t81
```

或使用 VSCode PlatformIO 插件，底部状态栏选择对应环境后点击 ✓ 编译。

### 烧录

```bash
# 编译并烧录
~/.platformio/penv/bin/pio run -e wft0420cz15 -t upload

# 编译 + 烧录 + 打开串口监视器
~/.platformio/penv/bin/pio run -e wft0420cz15 -t upload && ~/.platformio/penv/bin/pio device monitor
```

串口波特率: 115200

### 手动进入下载模式

如果自动烧录失败：
1. 按住 BOOT 键
2. 点按 EN (RST) 键
3. 松开 EN
4. 松开 BOOT
5. 开始烧录

## 项目结构

```
src/
├── main.cpp              # 主入口、状态机
├── config.h              # 引脚定义、常量、编译配置
│
├── display/
│   ├── display.h         # 屏幕能力、逻辑颜色抽象
│   └── display_gxepd2.*  # GxEPD2 显示管理器
│
├── fonts/
│   ├── inter_*.h         # Inter GFXfont 字体
│   └── cn_12.h           # 中文日期小字库
│
├── ui/
│   ├── widgets_gx.*      # Adafruit_GFX UI 组件
│   └── ui_quota.*        # 配额看板、Setup、Error 页面
│
├── battery.h/cpp         # 电池电压读取 + 百分比估算
├── button.h/cpp          # 按键检测 (短按/长按)
├── led_control.h/cpp     # LED 统一控制 (活动引用计数 + 状态闪烁)
├── power.h/cpp           # 深睡眠管理 (定时唤醒 + GPIO3 按键唤醒)
├── wifi_manager.h/cpp    # WiFi 连接管理
├── storage.h/cpp         # NVS 非易失存储 (WiFi配置、智控台网关、休眠间隔)
├── portal.h/cpp          # WiFi 配网门户 (SoftAP + Web Server)
├── http_client.h/cpp     # 智控台 HTTP 通信 (心跳/任务/对时)
├── quota.h/cpp           # 配额 API 直连和 JSON 解析
└── certs.h               # HTTPS 根证书

data/
└── portal_html.h         # 配网页面 HTML (单文件，内嵌 CSS/JS)
```

## 系统流程

### 启动流程

```
上电 / 定时唤醒
  │
  ├── 使用构建时注入的 Git 短 SHA，确认固件版本
  ├── 初始化 ADC、GPIO、LED
  ├── 检查唤醒原因 (boot / timer / button)
  ├── 读取电池电压
  │
  ├── 开机按住 GPIO3 按键？
  │   └── 是 → 进入配网模式 (Portal)
  │
  ├── NVS 中有 WiFi 配置？
  │   └── 否 → 进入配网模式 (Portal)
  │
  ├── 连接 WiFi (超时 15 秒)
  │   └── 失败 → 进入配网模式 (Portal)
  │
  ├── ONLINE:
  │   ├── 按策略 GET 智控台 /api/time 对时
  │   │   ├── 首次上电/手动重启：立即对时
  │   │   ├── 定时自动唤醒：距离上次成功对时 ≥ 4 小时才对时
  │   │   └── 按键唤醒：不自动对时，仅执行下发的 sync_time 任务
  │   ├── GET  http://YOUR_SERVER_IP:12001/LimiT/quErY 获取配额
  │   ├── POST {cfgConsoleUrl}/api/heartbeat 上报心跳
  │   ├── GET  {cfgConsoleUrl}/api/tasks?mac=... 拉任务
  │   ├── 执行 set_sleep / sync_time / restart
  │   └── 上报 POST /api/tasks/{id}/result
  │
  ├── 关闭 WiFi 射频
  │
  ├── 显示配额看板或设备状态页
  │
  └── 自动模式 2 秒后进入深睡眠；按键唤醒交互模式 30 秒后进入深睡眠
```

### 深睡眠

- **定时唤醒**: 默认每 5 分钟唤醒一次 (可通过 NVS 配置)
- **GPIO 唤醒**: GPIO 3 (按键) 低电平唤醒
- 唤醒后读取电池、按需联网、刷新屏幕；联网阶段结束后立即关闭 WiFi，按键切页使用 RTC/内存缓存数据

### 状态机

```
BOOT → 有WiFi配置? ─否→ PORTAL (配网)
         │
         是
         │
    CONNECTING ─失败→ PORTAL
         │
         成功
         │
      ONLINE
  (配额→心跳→任务)
         │
    DISPLAYING
 (自动2秒/交互30秒)
         │
      超时
         │
    SLEEPING → 定时/按键唤醒 → BOOT
```

### 低功耗联网策略

固件目前保留既有 HTTP 调用次数：配额、心跳、任务、任务结果、对时仍按原接口分别请求，不做合并。低功耗优化集中在“少开 WiFi、快连 WiFi、及时关 WiFi、少刷屏”：

- `wifi_manager.cpp` 会保存上次成功连接的 BSSID 和 channel，下次优先用 `WiFi.begin(ssid, pass, channel, bssid)` 定向重连，减少全信道扫描时间。
- `ONLINE` 阶段用 `WiFiSessionGuard` 包住完整联网流程。配额、心跳、任务和任务结果处理完成后调用 `stopWiFi()`，随后显示刷新和按键切页只使用缓存数据。
- 自动唤醒如果数据与上次显示完全一致，会跳过墨水屏刷新并立即深睡；Device 页和按键切页仍强制刷新。
- 电量低于等于 10% 且已有有效屏幕内容时，自动唤醒跳过联网，按低电量休眠规则延长睡眠。按键唤醒不走这个跳过逻辑，方便用户手动查看或进入配网。
- 低电量下配网热点超时从 5 分钟缩短到 2 分钟，避免 SoftAP 长时间耗电。

对时策略：首次上电、手动重启后自动对时；自动定时唤醒每隔 4 小时对时一次；智控台下发 `sync_time` 任务时立即对时；按键唤醒不会额外自动对时。

### 网络自愈与线性退避休眠系统

为了让设备能够在各种恶劣的网络和物理环境下做到 **100% 逻辑闭环** 并实现 **超长待机**，固件内置了创新的“自适应网络自愈与线性退避休眠系统”。

#### 1. 低功耗退避设计的必要性
ESP32-C3 在 WiFi 模式下的工作电流高达 `200mA ~ 240mA`。如果路由器因断电、改密或宽带欠费导致设备无法访问网络：
* **传统逻辑** 会导致设备不断高频重试，或无限期卡在 AP 配网热点状态，导致锂电池在几小时内被彻底耗尽。
* **退避系统** 会在判定网络异常后，让设备进入极低功耗的深度休眠状态（芯片功耗降至约 `5μA`），从而将电池寿命延长上万倍。

#### 2. 自适应自愈与配网转换流程
* **全流程健康判定**：设备唤醒后重连 Station WiFi。如果 **WiFi 连接失败**、**拉取配额数据失败** 或 **向智控台发送心跳包失败**，一律判定为断网。自动唤醒时直接进入退避深睡，按键交互时才切换到 `PORTAL` 配网模式。
* **配网超时**：开启 `PORTAL` 配网热点后，系统启动 5 分钟软件定时器；电量 ≤20% 时缩短为 2 分钟。超时仍未配网则进入退避休眠状态。
* **优先重试与清零**：当下一次退避休眠唤醒（例如睡了 10 分钟或 3 小时后），设备**绝对不会盲目开启热点**，而是**首先使用保存的 WiFi 配置重连并尝试进行心跳与数据获取**。一旦成功，即宣告网络自愈，失败计数器瞬间清零，设备完美退出退避循环。

#### 3. 线性退避计算与封顶规则
使用跨深睡眠持久化的 RTC 内存变量 `rtcWifiFailCount` 跟踪连续联网失败次数。为了防止休眠时间过快膨胀，同时避免设备频繁请求，休眠时间采用**线性级递增**（每次递增 2.5 分钟），并设置了 3 小时的合理上限：
* **第 1 次失败**：休眠 **5 分钟**
* **第 2 次失败**：休眠 **7 分钟**（实测约 7.5 分钟）
* **第 3 次失败**：休眠 **10 分钟**
* **第 4 次失败**：休眠 **12 分钟**（实测约 12.5 分钟）
* **第 5 次失败**：休眠 **15 分钟**
* **第 6 次失败**：休眠 **17 分钟**（实测约 17.5 分钟）
* **第 7 次失败**：休眠 **20 分钟**
* ...
* **多次失败后封顶**：固定在最大上限 **3 小时 (180 分钟)** 封顶。

> [!NOTE]
> 每次成功连接 WiFi 且心跳包和配额数据顺利接收后，`rtcWifiFailCount` 会立即被清零，重归常规的刷新时间间隔。

#### 4. 休眠屏提示与物理按键打断
* **极致的人机交互**：在休眠前，墨水屏会被更新为专属的 **“自适应休眠提示页面”**（由 `uiShowSleepPrompt()` 绘制）。页面包含醒目的大字号退避休眠时间，以及说明文字：*“网络故障，深度休眠中... 下次唤醒剩余 X min，可短按 CFG 键手动打断并立即配网”*。
* **随时打断**：在长达数小时乃至 24 小时的深度休眠期间，用户无需被动等待。只需短按物理 `CFG` 按键（GPIO3），即可立刻通过外部中断拉低电平唤醒芯片，芯片将瞬间复位并强制启动 `PORTAL` 配网页，让用户拥有 100% 的即时掌控权。

#### 5. 与常规休眠控制的无冲突解耦
系统在数据流和控制流上均实现了完美的解耦隔离，绝无冲突：
* **数据层面解耦**：退避休眠使用的是动态计算的 `backoffMin` 参数，而常规范式刷新休眠使用的是保存在闪存中的常规间隔 `cfgSleepMin`。二者物理存储独立，互不覆盖。
* **控制层面解耦**：当状态机处于 `PORTAL` 配网模式时，常规的 2 秒自动休眠 Timer 被禁用，交由配网超时软件定时器接管，确保用户有足够时间进行交互与输入。

---
## 按键操作

系统采用高灵敏度按键状态机，将物理交互升级为三级精细化管理：

| 操作 | 条件 | 动作 |
|---|---|---|
| **按住 GPIO3 开机** | 上电或复位时（插线瞬时/反应延迟双阶段检测） | 强制进入 AP 配网模式（WiFi Setup） |
| **短按 (< 1.2s)** | `DISPLAYING` 状态 | 切换墨水屏页面：Overview → Google → Codex → Zhipu → Device |
| **长按 (≥ 2s)** | `DISPLAYING` 状态 | **手动数据刷新**：连接 WiFi 自动感知并更新当前页数据（有差异才刷新屏幕，无差异仅快速三闪 LED） |
| **超长按 (≥ 8s)** | `DISPLAYING` 状态 | 强制进入 AP 配网模式（WiFi Setup） |
| **长按 (≥ 8s)** | 配网中 | 强制重启设备 |

### 手动数据刷新机制 (Manual Data Refresh)
当在看板唤醒期间**长按 2 秒**时，设备会自动唤醒 WiFi 并进行“页面感知式数据拉取”：
* **Overview 总览页**：直连拉取全部 3 个服务商配额（`fetchAllQuota`）。
* **供应商详情页**：只获取当前页面所对应 provider 的配额数据（`fetchProviderQuota`），最大程度节约功耗与等待时间。
* **Device 设备页**：执行系统时间同步（NTP）并向控制台发送最新状态心跳包。
* **差异化刷屏防护 (Difference check)**：
  * **有差异**：最新获取的数据与当前屏幕内容不同，设备执行局刷/全刷，更新数据，并在刷新后 **LED 双闪** 确认。
  * **无差异**：最新获取的数据无变化，为了极端省电并保护墨水屏，设备**自动跳过屏幕刷新**，仅通过 **LED 极速三闪** 告知用户。

深睡眠期间 MCU 不运行代码，没有短按/长按区别。GPIO3 拉低只负责唤醒芯片；唤醒后如果启动时仍按住 GPIO3，则进入配网模式。

---
## LED 指示灯视觉语言 (LED Visual Language)

板载指示灯 `PIN_LED` (GPIO 5, HIGH=亮, LOW=灭) 配合系统生命周期，设计了 **8 种极具辨识度的指示模式**，为用户提供清晰的设备状态感知。

LED 的 GPIO 写入集中在 `src/led_control.h/cpp`，业务模块不应直接调用 `digitalWrite(PIN_LED, ...)`。HTTP 数据交互、屏幕刷新、故障提示这类“长亮型活动”统一通过活动引用计数控制，避免多个模块同时控制 LED 时互相提前关灯。

### 指示灯模式一览

| # | 设备状态 | LED 模式 | 时序参数 | 触发场景 |
|---|---|---|---|---|
| 1 | **唤醒瞬时确认** | 极速快闪 3 次 | 亮 80ms / 灭 80ms | 定时唤醒、按键唤醒、上电复位，初始化完成后立即触发 |
| 2 | **WiFi 连接中** | 平缓慢闪 (呼吸态) | 亮 300ms / 灭 300ms | WiFi 连接等待期间，LED 电平每 300ms 自动翻转 |
| 3 | **HTTP 数据交互中** | 常亮 | 请求开始亮 → 请求结束灭 | 拉配额、心跳、拉任务、任务结果上报、对时期间；表示正在数据交互，不建议中断 |
| 4 | **联网成功 & 可交互** | 高级双闪 | 亮 150ms / 灭 150ms × 2 | 数据拉取成功、首帧屏幕刷新完毕后触发 |
| 5 | **屏幕刷新中** | 常亮 | 刷新开始亮 → 结束灭 | 任何屏幕全刷/局刷期间，由驱动层自动控制 |
| 6 | **误触死区静默** | 常灭 (无反馈) | - | 按键释放在 3s ~ 10s 死区内，不产生任何 LED 动作 |
| 7 | **AP 配网热点激活** | 科技感三闪循环 | 快闪 3 次 → 熄灭 1.2s → 循环 | 进入 SoftAP 配网模式后，非阻塞循环驱动 |
| 8 | **网络故障警告** | 长亮 2 秒 | 亮 2000ms → 灭 | WiFi 多次连接失败或数据同步超时，进入配网前警告 |

### LED 统一控制模块

`src/led_control.h/cpp` 提供两类接口：

| 接口 | 用途 |
|---|---|
| `ledControlInit()` | 初始化 LED GPIO，并清零活动计数 |
| `ledActivityBegin()` / `ledActivityEnd()` | 长亮型活动的引用计数入口；计数从 0 到 1 时亮灯，回到 0 时灭灯 |
| `LedActivityGuard` / `HttpLedGuard` | RAII 包装，函数提前返回时也能自动释放活动计数 |
| `ledBlink()` / `ledBlinkN()` | 唤醒确认、联网成功等短促闪烁；若已有长亮活动，则保持长亮并只等待对应时长 |
| `ledToggleIdle()` / `ledSetIdle()` / `ledOffIfIdle()` | WiFi 连接、配网门户等空闲态闪烁；只在没有长亮活动时改变 LED 电平 |
| `ledForceOff()` | 休眠、重启前强制清零活动计数并熄灯 |

长亮型活动允许重叠，例如 HTTP 请求尚未结束时触发屏幕刷新：

```text
HTTP 开始       activeCount = 1  LED ON
屏幕刷新开始    activeCount = 2  LED 保持 ON
HTTP 结束       activeCount = 1  LED 仍保持 ON
屏幕刷新结束    activeCount = 0  LED OFF
```

因此新增 LED 行为时按规则接入：

1. 会持续一段时间、且可能与 HTTP/屏刷重叠的状态，用 `ledActivityBegin()` / `ledActivityEnd()` 或 `LedActivityGuard`。
2. 只用于空闲态提示的闪烁，用 `ledSetIdle()`、`ledToggleIdle()`、`ledOffIfIdle()`。
3. 休眠或重启前需要确保灭灯，用 `ledForceOff()`。
4. 不要在业务模块中直接写 `PIN_LED`。

### 屏幕刷新常亮 — 驱动层自动接管

规范 #5 的 LED 控制已下沉至显示驱动底层 `display_gxepd2.cpp`，但实际 GPIO 写入仍由 `led_control` 统一管理：

```cpp
void displayPrepareFull() {
    ledActivityBegin();  // 全刷开始 → 活动计数 +1
    display.setFullWindow();
    display.firstPage();
}

void displayPreparePartial(int x, int y, int w, int h) {
    ledActivityBegin();  // 局刷开始 → 活动计数 +1
    // ...
}

bool displayNextPage() {
    bool hasNext = display.nextPage();
    if (!hasNext) {
        ledActivityEnd();  // 绘制结束 → 活动计数 -1，归零才灭灯
    }
    return hasNext;
}
```

这意味着系统中**任何位置、任何场景**触发的屏幕刷新（数据同步页、切页、配网页、休眠提示页等）都会自动获得 LED 亮灭反馈，无需在业务层逐一手动管理；同时即使与 HTTP 数据交互重叠，也不会互相抢灯。

### 墨水屏底栏动态状态徽章

利用墨水屏零功耗长效显示的硬件特性，在底栏中心位置渲染动态状态标志，辅助 LED 提供设备状态认知：

| 标志 | 含义 | 触发条件 |
|---|---|---|
| `[ON]` | 设备运行中，可交互 | 按键唤醒、短按切页等手动交互场景 |
| `[Zz]` | 设备处于深度休眠 | 定时唤醒的全自动流程，或手动交互结束进入休眠前 |

底栏完整布局：`左侧 Sleep:Xm` | `中间 [ON]/[Zz]` | `右侧 页码/总页数`

**省电策略**：
- **定时自动唤醒**：全程 `[Zz]`，仅执行 1 次屏幕刷新，超时后不作二次刷新直接休眠
- **按键手动交互**：运行时显示 `[ON]`，进入休眠前执行 1 次刷新将底栏更新为 `[Zz]`（黑白屏走局刷 0.3s，三色屏安全降级全刷）

## WiFi 配网 (Captive Portal)

### 流程

1. 设备启动 AP 热点: `Elnk-XXXX` (MAC 后 5 位)
2. 屏幕显示 "SETUP WIFI" + AP 名 + "OPEN BROWSER"
3. 手机连接 AP
4. 浏览器自动弹出或手动访问 `http://192.168.4.1`
5. 选择 WiFi、输入密码
6. 连接成功后自动保存到 NVS，10 秒后重启

### 配网网页功能

- WiFi 扫描 (按信号强度排序)
- WiFi 连接状态轮询
- 设备信息 (MAC、电池电压)
- 高级设置中可切换智控台网关地址
- 中英双语切换

### 重置 WiFi

1. 长按 GPIO3 按键 10 秒 → 设备重启
2. 重启过程中**继续按住**按键 → 强制进入配网模式
3. 在配网页面输入新的 WiFi 信息

## 智控台与远程任务

### 智控台网关配置

固件内置默认智控台地址：

```cpp
#define DEFAULT_CONSOLE_URL "http://your-console.example.com"
```

设备实际使用的地址由运行时变量 `cfgConsoleUrl` 决定，启动时从 NVS 读取：

1. NVS 中存在 `console_url`：使用保存的地址。
2. NVS 中为空：使用 `DEFAULT_CONSOLE_URL`。
3. 配网 Portal 的“高级设置 / 智控台地址”会显示当前地址，保存 WiFi 时一起写入 NVS。
4. 输入 `192.0.2.10:9857` 这类不带协议的地址会自动规范化为 `http://192.0.2.10:9857`。
5. 清空输入框并保存会恢复默认网关 `http://your-console.example.com`。

这样可以在本地调试、测试环境和生产域名之间切换，而不需要重新编译或烧录固件。长按 GPIO3 进入配网模式后即可修改该地址。

配置保存位置：

```text
NVS namespace: eink
key: console_url
default: http://your-console.example.com
```

### 设备调用接口

以下接口中的 `{cfgConsoleUrl}` 是设备当前选择的智控台网关，默认值为 `http://your-console.example.com`。

| 方法 | URL | 调用时机 |
|---|---|---|
| `POST` | `{cfgConsoleUrl}/api/heartbeat` | 每次唤醒，上报状态 |
| `GET` | `{cfgConsoleUrl}/api/tasks?mac=...` | 心跳后拉取任务 |
| `POST` | `{cfgConsoleUrl}/api/tasks/{id}/result` | 每个任务执行后上报结果 |
| `GET` | `{cfgConsoleUrl}/api/time` | 首次上电/手动重启、自动唤醒每 4 小时、或 `sync_time` 任务时对时 |

心跳上报字段：

```json
{
  "mac": "A1B2C3D4E5F6",
  "voltage": 4.10,
  "pct": 95,
  "ip": "192.0.2.10",
  "wake": "boot",
  "uptime_s": 40,
  "device_ts": "2026-05-20 09:23",
  "free_heap": 84000,
  "rssi": -45,
  "firmware": "g1a2b3c4",
  "sleep_min": 5
}
```

支持任务：

| type | params | 动作 |
|---|---|---|
| `set_sleep` | `{"minutes":10}` | 保存新的深睡眠唤醒间隔 |
| `sync_time` | `{}` | 请求 `/api/time` 并设置系统时间 |
| `restart` | `{}` | 上报结果后立即 `ESP.restart()` |

## 电池监测

### 硬件

GPIO0 通过 R1=10k / R2=10k 分压电阻连接电池：

```
BAT ──[10k]── GPIO0 ──[10k]── GND
```

### 电压计算

```cpp
// 16 次采样，排序后去掉最高最低各 2 个，取均值
// esp_adc_cal 校准，12 位分辨率，11dB 衰减 (0-3.3V 量程)
// 真实电压 = ADC 引脚电压 × 2 (分压比)
```

### 电量估算

当前 `readBatteryVoltage()` 直接返回真实电池电压，不再映射到 0~3.3V。`batteryPercent()` 使用锂电池放电曲线做分段估算：

| 真实电压 | 估算电量 |
|---|---|
| ≥ 4.20V | 100% |
| 4.10V | ~95% |
| 4.00V | ~82% |
| 3.90V | ~68% |
| 3.80V | ~50% |
| 3.70V | ~30% |
| 3.60V | ~15% |
| 3.50V | ~5% |
| ≤ 3.30V | 0% |

### 串口输出

```
Battery: 3.85V (59%)
```

## 显示系统

### 当前显示栈

屏幕层使用 GxEPD2 / Adafruit_GFX：

| 层级 | 文件 | 职责 |
|---|---|---|
| 屏幕能力抽象 | `src/display/display.h` | `DisplayCaps`、逻辑颜色 `InkColor` |
| GxEPD2 适配 | `src/display/display_gxepd2.h/cpp` | 创建对应屏幕对象、初始化 SPI、全刷/局刷入口、颜色映射 |
| UI 组件 | `src/ui/widgets_gx.h/cpp` | 文本测量、截断、居中、进度条、卡片、中文日期混排 |
| 页面绘制 | `src/ui/ui_quota.h/cpp` | Overview / Google / Codex / Zhipu / Device / Setup / Error 页面 |
| 字体 | `src/fonts/*.h` | Inter `GFXfont` + 中文日期小字库 |

当前 `platformio.ini` 依赖：

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0
    zinggjm/GxEPD2@^1.6.9
```

两个屏幕通过编译宏选择：

| 环境 | 编译宏 | GxEPD2 类型 | 能力 |
|---|---|---|---|
| `wft0420cz15` | `DISPLAY_WFT0420CZ15` | `GxEPD2_3C<GxEPD2_420c, ...>` | 400x300，黑/白/红，全刷 |
| `gdey042t81` | `DISPLAY_GDEY042T81` | `GxEPD2_BW<GxEPD2_420_GDEY042T81, ...>` | 400x300，黑/白，支持局刷能力标记 |

显示相关实现统一在 `src/display/`、`src/ui/`、`src/fonts/` 中维护。

### 字体

当前有两套实际绘制字体：

1. **Inter `GFXfont`**：英文、数字、百分号、冒号、斜杠等 ASCII UI 文本，走 Adafruit_GFX `setFont()` / `print()`。
2. **中文日期小字库 `cn_12.h`**：只包含日期栏需要的少量中文，如 `月`、`日`、`周`、`一二三四五六`，以及时段词 `凌晨`、`清晨`、`上午`、`中午`、`下午`、`晚上`，由 `drawMixedDateCnGx()` 混排绘制。

粗体/半粗体全部来自真实 Inter TTF 转换结果，不做算法加粗。

Inter 字体源文件位于：

```text
firmware/tools/fonts/Inter/
```

其中包含 Inter Variable 原始字体和当前转换用的 static TTF。Variable Font 用于以后重新选择字重、字号或字符集；当前固件生成使用 `Inter-18pt-Regular.ttf`、`Inter-24pt-Regular.ttf`、`Inter-24pt-SemiBold.ttf`、`Inter-24pt-Bold.ttf`。

转换脚本：

```bash
cd firmware
python3 tools/convert_inter_to_gfxfont.py
python3 tools/convert_cn_to_gfxfont.py
```

生成文件：

```text
src/fonts/inter_regular_11.h
src/fonts/inter_regular_14.h
src/fonts/inter_semibold_18.h
src/fonts/inter_bold_28.h
src/fonts/cn_12.h
```

当前已生成：

| 字体符号 | 来源 | 用途建议 |
|---|---|---|
| `InterRegular11pt7b` | `Inter-18pt-Regular.ttf` | 小标签、重置时间、辅助信息 |
| `InterRegular14pt7b` | `Inter-24pt-Regular.ttf` | 普通信息 |
| `InterSemiBold18pt7b` | `Inter-24pt-SemiBold.ttf` | 小标题、provider 名称 |
| `InterBold28pt7b` | `Inter-24pt-Bold.ttf` | 大数字、电量百分比 |

Inter 转换范围是 ASCII `0x20~0x7E`，包含英文大小写、数字和常见英文 UI 特殊字符，例如 `%`、`:`、`/`、`-`、`.`、`(`、`)`、`@`、`_`。Inter 原始字体包含更多字符，但没有导出的字符不会进入固件。

> 注意：Adafruit_GFX 的 `GFXfont` 位图要求每个 glyph 按连续 bit 流打包，不能按行补齐字节。`tools/convert_inter_to_gfxfont.py` 已按连续 bit 流生成。若后续英文/特殊字符出现“糊成一片”，优先检查是否误把 Inter 字体改回了按行补齐格式。

当前 Inter bitmap payload 约 `7169 bytes`，TTF 源文件不会被编译进固件。配额看板页、配网页、状态页和错误页已切换到 GxEPD2 / Adafruit_GFX 绘制；长文本会自动截断，避免溢出屏幕。最近编译验证：

| 环境 | RAM | Flash |
|---|---:|---:|
| `wft0420cz15` | 71996 / 327680 bytes (22.0%) | 1026426 / 1966080 bytes (52.2%) |
| `gdey042t81` | 57004 / 327680 bytes (17.4%) | 1026804 / 1966080 bytes (52.2%) |

### 墨水屏页面

#### Setup 屏 (配网)

```
┌────────────────────────────────┐
│ ████████████████████████████████│  ← 红色顶栏，白色 Inter 标题
│                                │
│        Connect to WiFi AP       │
│                                │
│        Elnk-A1B2C               │  ← Inter Bold 28，过长自动截断
│                                │
│        Open browser             │
│        http://192.168.4.1       │
│                                │
└────────────────────────────────┘
```

#### 配额看板页

连接 WiFi 后进入 `ui_quota` 看板，共 5 页：

| 页码 | 页面 | 内容 |
|---|---|---|
| 0 | Overview | Google / Codex / Zhipu 配额总览 |
| 1 | Google | Gemini / Claude 配额详情 |
| 2 | Codex | H5 / W 配额详情 |
| 3 | Zhipu | H5 / W / MCP 配额详情 |
| 4 | Device | IP、MAC、电池、电量条、时间、唤醒原因、RSSI、heap、sleep、firmware Git SHA |

#### 页面切换

- 定时唤醒、按键唤醒都会从 RTC 内存恢复休眠前显示的页码；休眠前停在哪一页，唤醒后仍检查并显示哪一页
- 上电/复位会回到页码 0（Overview），同时清空显示快照
- 短按 GPIO3 依次切换：0 → 1 → 2 → 3 → 4 → 0 ...
- 长按 GPIO3 进入配网模式

#### 屏幕刷新策略

为减少墨水屏不必要的刷新（节省时间、减少闪烁），唤醒后首次显示时会比较当前数据与 RTC 缓存的上次显示快照：

- **配额页（0~3）**：对比当前页面实际显示的数据 + 电量百分比 + 状态栏日期时段（凌晨/清晨/上午/中午/下午/晚上）+ 休眠分钟数 `sleep_min` + 底栏 `[ON]/[Zz]` 状态。Overview 页比较完整 `AllQuotaData`；Google / Codex / Zhipu 详情页只比较对应 provider。全部相同才跳过刷新，串口输出 `Display unchanged, skipping refresh`，直接进入深睡眠
- **Device 页（4）**：始终刷新（内容区时间精确到分钟）
- **按键翻页**：始终刷新（`forceRefresh=true`）
- **首次启动/复位**：`rtcDisplayValid = false`，强制首次刷新

RTC 快照变量（深睡眠保持，断电清零）：

```cpp
RTC_DATA_ATTR AllQuotaData rtcQuota;       // 配额数据
RTC_DATA_ATTR int rtcLastBatteryPct;       // 上次显示的电量
RTC_DATA_ATTR int rtcLastSleepMin;         // 上次显示的休眠分钟数
RTC_DATA_ATTR char rtcLastDateStr[16];     // 上次状态栏日期时段快照
RTC_DATA_ATTR bool rtcLastInteractiveActive; // 上次显示的 [ON]/[Zz] 状态
RTC_DATA_ATTR bool rtcDisplayValid;        // 快照是否有效
RTC_DATA_ATTR int rtcLastPage;             // 上次显示的页码（用于唤醒恢复和对比）
```

## GxEPD2 颜色和刷新接口

当前固件通过逻辑颜色 `InkColor` 做屏幕颜色抽象，不直接操作底层 framebuffer：

| 逻辑颜色 | 三色屏 `wft0420cz15` | 黑白屏 `gdey042t81` |
|---|---|---|
| `InkColor::White` | `GxEPD_WHITE` | `GxEPD_WHITE` |
| `InkColor::Black` | `GxEPD_BLACK` | `GxEPD_BLACK` |
| `InkColor::Red` | `GxEPD_RED` | 降级为 `GxEPD_BLACK` |

显示门面位于 `src/display/display_gxepd2.h/cpp`：

```cpp
void displayBegin();
void displayPrepareFull();
void displayPreparePartial(int x, int y, int w, int h);
bool displayNextPage();
void displayClear(InkColor color);
void displayFullRefresh();
void displayPartialRefresh(int x, int y, int w, int h);
void displaySleep();
uint16_t displayColor(InkColor color);
const DisplayCaps &displayCaps();
EInkDisplay &displayCanvas();
```

页面绘制采用 GxEPD2 分页流程：

```cpp
displayPrepareFull();
do {
    displayClear(InkColor::White);
    // draw page with Adafruit_GFX API
} while (displayNextPage());
```

编译时通过 `DISPLAY_WFT0420CZ15` / `DISPLAY_GDEY042T81` 选择屏幕型号。

## 串口日志示例

### 正常启动 (定时唤醒)

```
[I main.cpp:232 setup] === eink built May 20 2026 09:23:35 ===
[I main.cpp:246 setup] Wakeup: timer
[I main.cpp:267 setup] Battery: 4.10V (95%)
[I wifi_manager.cpp:26 connectWiFi] WiFi connecting, attempt=1/3 ssid=ExampleWiFi
[I wifi_manager.cpp:51 connectWiFi] WiFi OK, ip=192.168.1.100 rssi=-45 elapsed=1200ms
-- Fetch quota --
[I quota.cpp:75 fetchAllQuota] Quota OK, providers=3
[I main.cpp:347 loop] Quota phase elapsed=760ms
[I http_client.cpp:63 sendHeartbeat] Heartbeat OK, sleep_min=5
[I http_client.cpp:112 fetchPendingTasks] Tasks OK, count=0
[I wifi_manager.cpp:66 stopWiFi] WiFi off, elapsed=106ms
[I main.cpp:437 loop] Device time: 2026-05-20 09:23:40
[I main.cpp:442 loop] Display refresh phase elapsed=4910ms
[I main.cpp:482 loop] Display done (auto mode, 2001ms), sleeping 5 min
[I power.cpp:6 enterDeepSleep] Deep sleep for 5 min
```

### 首次启动 (无 WiFi 配置)

```
[I main.cpp:232 setup] === eink built May 20 2026 09:23:35 ===
[I main.cpp:258 setup] Wakeup: boot/reset
[I main.cpp:267 setup] Battery: 3.85V (59%)
[I main.cpp:290 setup] No WiFi config -> portal
[I portal.cpp:44 startCaptivePortal] AP started: Elnk-A1B2C ip=192.168.4.1
[I portal.cpp:177 startCaptivePortal] Captive portal started
... (手机配网) ...
[I portal.cpp:119 operator()] Portal connecting to ssid=ExampleWiFi
[I portal.cpp:146 operator()] Portal WiFi OK, ip=192.168.1.100
[I portal.cpp:184 handlePortalClients] Portal: deferred restart
```

### 按键重启进配网

```
[I main.cpp:232 setup] === eink built May 20 2026 09:23:35 ===
[I main.cpp:258 setup] Wakeup: boot/reset
[I main.cpp:267 setup] Battery: 3.80V (50%)
[I main.cpp:272 setup] Button held -> portal
[I portal.cpp:44 startCaptivePortal] AP started: Elnk-A1B2C ip=192.168.4.1
[I portal.cpp:177 startCaptivePortal] Captive portal started
```

## platformio.ini

```ini
[platformio]
default_envs = wft0420cz15

[common]
platform = espressif32
framework = arduino
board = esp32-c3-devkitm-1
monitor_speed = 115200
board_build.partitions = min_spiffs.csv
lib_deps =
    bblanchon/ArduinoJson@^7.0
    zinggjm/GxEPD2@^1.6.9

[env:wft0420cz15]
extends = common
build_flags =
    -DDISPLAY_WFT0420CZ15
    -DEPD_WIDTH=400 -DEPD_HEIGHT=300
    -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1

[env:gdey042t81]
extends = common
build_flags =
    -DDISPLAY_GDEY042T81
    -DEPD_WIDTH=400 -DEPD_HEIGHT=300
    -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1
```

## 配额 API

数据来源: [ModelLimitQuery](http://YOUR_SERVER_IP:12001/LimiT/quErY) 服务

### 请求

```
GET /LimiT/quErY
```

一次请求返回所有 provider，无需按 provider 分别调用。

### 响应

```json
[
  {"cl":100,"clr":"21:55","gm":100,"gmr":"21:55","p":"gemini","t":1779180905},
  {"h5":99,"h5r":"21:50","p":"codex","t":1779180642,"w":89,"wr":"05-25"},
  {"h5":46,"h5r":"19:51","mcp":99,"mcpr":"06-02","p":"zhipu","t":1779180906,"w":60,"wr":"05-21"}
]
```

### 字段说明

| 字段 | 含义 | 适用 provider |
|---|---|---|
| `p` | 平台标识 | 全部 |
| `t` | 数据获取时间 (epoch) | 全部 |
| `h5` | 5小时窗口剩余 % | codex, zhipu |
| `h5r` | 5小时窗口重置时间 | codex, zhipu |
| `w` | 每周剩余 % | codex, zhipu |
| `wr` | 每周重置时间 | codex, zhipu |
| `mcp` | MCP 每月剩余 % | zhipu |
| `mcpr` | MCP 每月重置时间 | zhipu |
| `gm` | Gemini 剩余 % | gemini |
| `gmr` | Gemini 重置时间 (UTC+8) | gemini |
| `cl` | Claude 剩余 % | gemini |
| `clr` | Claude 重置时间 (UTC+8) | gemini |

## 当前实现状态

已实现：

1. **WiFi 配网**: SoftAP + Captive Portal，保存 WiFi 到 NVS
2. **深睡眠唤醒**: 定时唤醒 + GPIO3 低电平按键唤醒
3. **电池监测**: ADC 校准读取真实电压，按锂电曲线估算百分比
4. **配额直连**: 每次唤醒直连 `YOUR_SERVER_IP:12001/LimiT/quErY`
5. **智控台心跳**: 上报电压、电量、IP、wake、heap、RSSI、firmware Git SHA、sleep_min
6. **任务执行**: 支持 `set_sleep` / `sync_time` / `restart`
7. **配额 UI**: 五页墨水屏看板，短按切换页面，RTC 缓存配额和显示快照
8. **可配置智控台网关**: 默认 `http://your-console.example.com`，可在配网 Portal 高级设置中切换并保存到 NVS
9. **Inter 字体**: TTF 离线转 Adafruit_GFX `GFXfont`，4 套字号 (Regular11/14, SemiBold18, Bold28)
10. **屏幕刷新优化**: 定时唤醒后对比配额+电量+日期时段，数据未变则跳过刷新；Device 页始终刷新
11. **页面切换**: 深睡眠唤醒恢复休眠前页码；上电/复位回到 Overview；按键直接翻到下一页
12. **LED 状态语言**: WiFi 连接慢闪；HTTP 数据交互常亮；屏幕刷新常亮；成功后双闪
13. **低功耗 WiFi 生命周期**: 保存 BSSID/channel 快连，联网阶段结束后立即关闭 WiFi，自动无刷新时直接深睡
14. **分级对时策略**: 首次上电/手动重启立即对时，自动唤醒每 4 小时对时，`sync_time` 任务立即对时
15. **定位型串口日志**: `LOGI/LOGW/LOGE` 自动打印级别、文件名、行号和函数名，可通过 `LOG_LEVEL` 控制编译输出

待验证/待优化：

1. **阿里云联调**: 确认 nginx 域名反代后 ESP32 能访问 `/api/*`
2. **任务幂等**: 长时间验证 `pending → sent → done/failed` 不会重复执行 `restart`
3. **离线缓存**: 当前配额缓存使用 RTC 内存，断电后会丢；如需断电缓存，可再写入 NVS
4. **SSD1683 驱动**: 等待提供 GDEY042T81 屏幕初始化序列
5. **长期功耗曲线**: 需要实机长时间记录 WiFi 快连耗时、跳过刷新次数、低电量休眠间隔和真实续航
