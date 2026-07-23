# ESP32-P4 水位识别摄像头验证工程

这个工程用于把 STM32H7 工程中的 `APP_MODE_XCAM_VIEW` 摄像头链路先迁移到 ESP32-P4。当前摄像头基线已经跑通，后续主线是模型部署、摄像头与水泵联动、语音和按键、C6 串口接入，最后再处理 OLED、身份识别等其他模块。

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

完整引脚表见 [docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx](docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx)，工程规则见 [docs/BLUEPRINT_RULE.md](docs/BLUEPRINT_RULE.md)。

## 构建与烧录

```powershell
cd C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view
. .\scripts\idf-env.ps1
$env:CCACHE_DISABLE='1'
idf.py build
idf.py -p COM25 -b 460800 flash
```

## 项目记录

- 主蓝图和错误记录：[docs/BLUEPRINT_RULE.md](docs/BLUEPRINT_RULE.md)
- 当前引脚 Excel：[docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx](docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx)

## 当前模型产物

- ESP-DL 模型：`models/waterlevel_v3_from_qat_float_int8.espdl`
- 量化信息：`models/waterlevel_v3_from_qat_float_int8.info`
- 复现配置：`models/waterlevel_v3_from_qat_float_int8.json`
- 转换记录：`models/waterlevel_v3_from_qat_float_int8_conversion_manifest.json`

这版模型在 PC 侧 PPQ 外部 `teste` 上的板端后处理精度为 `551/624 = 88.30%`，raw argmax 为 `562/624 = 90.06%`。当前可用于 P4 模型加载和摄像头推理链路冒烟测试；闭环控制时必须叠加多帧确认和安全状态机。

## 当前原则

1. 摄像头基线已经跑通，后续迁移不得随意改 RESET/PWDN 时序、DVP 引脚顺序、XCLK 设置和 OV2640 寄存器表。
2. GPIO0-GPIO15 当前不要接 3.3V 外设信号。DNESP32P4M 上这些脚属于 VDDPST1 域，之前实测只有约 1.2-1.35V。
3. 外置 ESP32-C6 保留为独立联网模块，通过 UART 和 P4 通信；C6 代码已完成，P4 侧后期只补串口协议。
4. 只提交阶段性成果；临时调试、小文档修订和中间试错不单独提交。
