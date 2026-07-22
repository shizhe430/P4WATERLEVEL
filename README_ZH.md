# ESP32-P4 水位识别摄像头验证工程

这个工程用于把 STM32H7 工程中的 `APP_MODE_XCAM_VIEW` 摄像头链路先迁移到 ESP32-P4。当前目标是先稳定跑通 OV2640 DVP 摄像头和 JPEG 输出，再逐步迁移外置 ESP32-C6 通信、泵控、语音、指纹、OLED 和完整养老饮水流程。

## 当前已验证状态

- 主控：ESP32-P4，目标芯片 `esp32p4`
- 框架：ESP-IDF v5.5.5
- 摄像头：ATK-MC2640 / OV2640 DVP 模块
- 图像：320x240 JPEG，摄像头原生压缩
- 时钟：摄像头模块自带有源晶振，ESP32-P4 不输出 XCLK
- 输出：UART0，921600 bit/s，连续发送完整 JPEG 帧
- 验证结果：20 秒采集到 144 帧完整 JPEG，图片可正常打开

## 关键接线

| 信号 | ESP32-P4 GPIO |
|---|---:|
| SCL | GPIO32 |
| SDA | GPIO33 |
| RESET/RST | GPIO16 |
| PWDN | GPIO17 |
| D0-D5 | GPIO18-GPIO23 |
| D6 | GPIO26 |
| D7 | GPIO27 |
| PCLK | GPIO28 |
| VSYNC | GPIO29 |
| HREF/DE | GPIO30 |
| UART0 TX/RX | GPIO37/GPIO38 |

完整引脚表见 [docs/PIN_MAPPING.md](docs/PIN_MAPPING.md)。

## 构建与烧录

```powershell
cd C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view
. .\scripts\idf-env.ps1
$env:CCACHE_DISABLE='1'
idf.py build
idf.py -p COM25 -b 460800 flash
```

## 项目记录

- 摄像头移植问题记录：[docs/ESP32P4_BRINGUP_NOTES.md](docs/ESP32P4_BRINGUP_NOTES.md)
- 不再重复踩坑清单：[docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)
- 引脚映射与模块审计：[docs/PIN_MAPPING.md](docs/PIN_MAPPING.md)
- 后续迁移流程：[docs/MIGRATION_FLOW.md](docs/MIGRATION_FLOW.md)

## 当前原则

1. 摄像头基线已经跑通，后续迁移不得随意改 RESET/PWDN 时序、DVP 引脚顺序、XCLK 设置和 OV2640 寄存器表。
2. GPIO0-GPIO15 当前不要接 3.3V 外设信号。DNESP32P4M 上这些脚属于 VDDPST1 域，之前实测只有约 1.2-1.35V。
3. 外置 ESP32-C6 保留为独立联网模块，通过 UART 和 P4 通信；一周内不把 P4 与 C6 合成一个新复杂工程。
4. 每迁移一个模块，都先完成单模块验证，再并入完整养老饮水状态机。
