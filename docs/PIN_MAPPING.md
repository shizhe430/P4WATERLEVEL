# ESP32-P4 引脚映射与模块审计

本表以 STM32H7 工程为来源，记录迁移到 ESP32-P4 后的当前引脚决策。摄像头已经实测成功；其他模块先做分配规则和风险审计，未确认 DNESP32P4M 板卡实际引出和电平前不写死到代码里。

## 总规则

1. GPIO0-GPIO15 当前不用于 3.3V 外设信号。DNESP32P4M 上这些脚属于 VDDPST1 域，之前实测约 1.2-1.35V。
2. 摄像头占用 GPIO16-GPIO33 中的大部分连续引脚，后续模块不得和摄像头冲突。
3. UART0 GPIO37/GPIO38 当前用于 JPEG 调试输出。最终整机可以释放或改成日志口，但在摄像头基线冻结前不要改。
4. 外置 ESP32-C6 已有联网代码，P4 侧只需要预留一组独立 UART。
5. 每次改 GPIO，必须同步修改 `sdkconfig.defaults.esp32p4`、`sdkconfig` 和本文档。

## OV2640 DVP 摄像头

| 摄像头信号 | STM32H7 来源 | ESP32-P4 GPIO | 配置项 | 状态 | 说明 |
|---|---|---:|---|---|---|
| SCL | PB3，软件 SCCB，开漏上拉 | GPIO32 | `CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP` | 已验证 | 使用内部上拉，按 STM32 SCCB 方式探测 |
| SDA | PB4，软件 SCCB，开漏上拉 | GPIO33 | `CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP` | 已验证 | 不用普通 I2C ACK 扫描做唯一判断 |
| RESET/RST | ST 端已验证控制时序 | GPIO16 | `CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN` | 已验证 | 保留 ST 端低脉冲复位时序 |
| PWDN | ST 端已验证控制时序 | GPIO17 | `CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN` | 已验证 | 先高后低，最终保持低 |
| D0 | PC6 / DCMI_D0 | GPIO18 | `CONFIG_EXAMPLE_DVP_D0_PIN` | 已验证 | 8-bit DVP |
| D1 | PC7 / DCMI_D1 | GPIO19 | `CONFIG_EXAMPLE_DVP_D1_PIN` | 已验证 | 8-bit DVP |
| D2 | PC8 / DCMI_D2 | GPIO20 | `CONFIG_EXAMPLE_DVP_D2_PIN` | 已验证 | 8-bit DVP |
| D3 | PC9 / DCMI_D3 | GPIO21 | `CONFIG_EXAMPLE_DVP_D3_PIN` | 已验证 | 8-bit DVP |
| D4 | PC11 / DCMI_D4 | GPIO22 | `CONFIG_EXAMPLE_DVP_D4_PIN` | 已验证 | 8-bit DVP |
| D5 | PD3 / DCMI_D5 | GPIO23 | `CONFIG_EXAMPLE_DVP_D5_PIN` | 已验证 | 8-bit DVP |
| D6 | PB8 / DCMI_D6 | GPIO26 | `CONFIG_EXAMPLE_DVP_D6_PIN` | 已验证 | 8-bit DVP |
| D7 | PB9 / DCMI_D7 | GPIO27 | `CONFIG_EXAMPLE_DVP_D7_PIN` | 已验证 | 8-bit DVP |
| PCLK | PA6 / DCMI_PIXCLK | GPIO28 | `CONFIG_EXAMPLE_DVP_PCLK_PIN` | 已验证 | 运行时有有效帧 |
| VSYNC | PB7 / DCMI_VSYNC | GPIO29 | `CONFIG_EXAMPLE_DVP_VSYNC_PIN` | 已验证 | 运行时有有效帧 |
| HREF/DE | PH8 / DCMI_HSYNC | GPIO30 | `CONFIG_EXAMPLE_DVP_DE_PIN` | 已验证 | 运行时有有效帧 |
| XCLK | 模块自带 24MHz 有源晶振 | 不接 | `CONFIG_EXAMPLE_DVP_XCLK_PIN=-1` | 已验证 | 不从 P4 输出 XCLK |
| JPEG 输出 TX | USART1_TX PA9，921600 | GPIO37 | `CONFIG_ESP_CONSOLE_UART_TX_GPIO` | 已验证 | 当前作为原始 JPEG 输出 |
| JPEG 输出 RX | USART1_RX PA10，921600 | GPIO38 | `CONFIG_ESP_CONSOLE_UART_RX_GPIO` | 已验证 | 当前作为控制台 RX |

## 其他模块审计

| 模块 | STM32H7 当前来源 | ESP32-P4 初步决策 | 配置风险 | 下一步验证 |
|---|---|---|---|---|
| 外置 ESP32-C6 联网模块 | USART3：PB10 TX、PB11 RX，115200；代码中向 `huart3` 上报指纹和出水完成事件 | 保留独立 C6，通过 P4 的一组新 UART 通信；暂不占用 GPIO37/38 | 不能用 GPIO0-GPIO15；需确认 DNESP32P4M 剩余引脚实际引出 | 先做 P4<->C6 串口收发自检，再迁移业务协议 |
| 指纹 AS608 | USART2：PA2 TX、PA3 RX，115200 起，多波特率握手；WAK 为 PE6 输入 | 需要一组 UART + 一个 WAK 输入 | 与 C6、调试串口不能冲突；WAK 输入需要确认电平和上拉 | 先移植 AS608 通信，再接入身份状态机 |
| 语音识别/播报模块 | I2C4：PD12 SCL、PD13 SDA；设备地址 `0x34`；播报寄存器 `0x6E`，识别结果 `0x64` | 需要一组 I2C，优先选确认 3.3V 的非摄像头 GPIO | 不能再接到 VDDPST1 低压脚；SCL/SDA 需要外部或内部上拉确认 | 先写 I2C 读写寄存器测试，避免再出现播报互相打断 |
| OLED | GPIO 模拟时序：PB13 SCL、PB15 SDA、PC1 CS、PC4 RES、PC5 DC | 可以继续用 GPIO bit-bang，也可改硬件 SPI；先倾向保留 bit-bang 降低迁移风险 | 需要 5 个普通输出，不能占摄像头和低压域 | 先点亮屏幕，再迁移养老状态显示 |
| 水箱液位输入 | PB5，普通输入无上下拉 | 一个普通输入 GPIO | 传感器输出电平需确认，避免低压域 | 先做空/有水两态读取 |
| 冷水按键 | PF7，输入上拉，低电平有效 | 一个输入上拉 GPIO | 按键去抖要保留 | 先做按键事件测试 |
| 泵 PWM/开关 | PH6，代码中 TIM12 PWM；快速模式会临时拉成 GPIO 高电平 | 优先用 P4 LEDC PWM 输出，默认上电保持关闭 | 涉及实际出水，必须先空载或断泵验证；默认状态必须安全 | 先只打印 PWM 指令，再接驱动板 |
| 氛围灯 | PH2、PH5，普通输出 | 两个普通输出或 LEDC 输出 | 低风险，但仍需避开摄像头脚 | 后置迁移 |
| 温度/DS18B20 | Excel 中有 DS18B20 DQ，当前代码未在本次审计中定位到完整驱动 | 一个 1-Wire GPIO | 时序敏感，需单独验证 | 等核心饮水逻辑稳定后迁移 |

## 暂不分配固定 GPIO 的原因

当前摄像头已经占用一组确定且能工作的 3.3V 引脚。剩余模块数量不少，如果现在只凭芯片 GPIO 编号分配，容易踩到开发板未引出、被板载器件占用、或电源域不对的问题。因此下一步先按模块接口类型建立驱动，再结合 DNESP32P4M 原理图和实测电平确定最终底板引脚。

优先级建议：

1. C6 UART：联网是乐鑫物联网赛道展示重点，且迁移风险低。
2. 泵控安全输出：必须先保证默认停水和异常停水策略正确。
3. 语音 I2C：之前出现过播报被异常打断，迁移时要把状态机和播报节流一起处理。
4. OLED、按键、液位、指纹：按单模块验证后并入主流程。
