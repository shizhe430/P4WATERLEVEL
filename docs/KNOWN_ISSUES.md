# ESP32-P4 迁移问题记录

这个文件用于记录已经踩过的坑和最终规则。后面只要硬件、引脚、配置或调试结论变化，都先更新这里，再改代码。

## 已确认不能再犯的问题

| 问题 | 现象 | 最终规则 |
|---|---|---|
| 把摄像头接到 GPIO0-GPIO15 | SCL/SDA 空闲只有约 1.2-1.35V，OV2640 无法被 SCCB 读到 | 这些脚属于 VDDPST1 域，未确认 3.3V 前不要接 3.3V 摄像头和外设 |
| 用 ACK 扫描判断 OV2640 是否存在 | 普通 I2C 扫描一直失败，容易误判摄像头坏或地址错 | OV2640 SCCB 按 STM32 方式读 MID/PID，不把 ACK 扫描作为唯一依据 |
| 随机尝试 RESET/PWDN 电平组合 | 已经打断过调试节奏，而且容易偏离 ST 端成功蓝图 | 保留 ST 端时序：PWDN 先高后低，RESET 低脉冲后拉高，延时按当前代码执行 |
| 给 ATK-MC2640 输出 XCLK | 模块本身带有源晶振，额外 XCLK 没必要，还可能引入干扰判断 | `CONFIG_EXAMPLE_DVP_XCLK_PIN=-1`，不接 ESP32-P4 XCLK |
| 日志和 JPEG 共用串口输出 | 上位机按 JPEG 头尾解析时会被文本日志污染 | 摄像头稳定启动后关闭日志，只输出完整 JPEG 二进制 |
| 直接调用 `uart_write_bytes()` | ESP-IDF 日志能输出不代表 UART driver API 已安装 | 原始 JPEG 输出前必须安装并配置 UART driver |
| 没处理空 JPEG buffer | ESP-IDF DVP 驱动可能在 0 字节帧上崩溃 | 保留本仓库 `patches/esp-idf-v5.5.5-dvp-empty-jpeg-guard.patch` 记录的 IDF 补丁 |
| 引脚表和 `sdkconfig` 不同步 | 固件打印的 SCL/SDA 还是旧引脚，实际接线已变 | 每次改 GPIO，同时更新 `sdkconfig.defaults.esp32p4`、`sdkconfig` 和 `docs/PIN_MAPPING.md` |

## 当前摄像头基线

- SCL GPIO32，SDA GPIO33
- RESET GPIO16，PWDN GPIO17
- D0-D5 GPIO18-GPIO23，D6 GPIO26，D7 GPIO27
- PCLK GPIO28，VSYNC GPIO29，HREF/DE GPIO30
- XCLK 不接
- UART0 TX GPIO37，RX GPIO38，921600 bit/s

## 新问题记录模板

每次新增问题按这个格式追加，尽量写结论，不写猜测：

```text
### YYYY-MM-DD 问题标题

- 现象：
- 已排除：
- 根因：
- 最终处理：
- 以后规则：
```
