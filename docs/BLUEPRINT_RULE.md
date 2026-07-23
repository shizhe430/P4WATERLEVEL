# ESP32-P4 WaterLevel 蓝图 Rule

本文档是本工程唯一主蓝图。后续推进工程时，先看本文件；新增错误、引脚变化、流程变化、模型部署结论都写回本文件，避免分散在多个 md 中被忽略。

单独的引脚 Excel 表为：`docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx`。

## 1. 当前硬规则

1. P4 是主控，外置 ESP32-C6 只负责联网，C6 已有代码，后期只做 UART 协议接入。
2. 摄像头基线已经跑通，OV2640 DVP 引脚、RESET/PWDN 时序、XCLK 设置不得随意改。
3. 先做模型部署，再做摄像头与水泵联动，再做语音和按键，再接 C6，最后做其他模块。
4. 只提交阶段性成果，不为小文档修改、临时调试、试错过程单独提交。
5. GPIO0-GPIO15 当前不接 3.3V 外设。VDDPST1 曾实测约 1.2-1.35V，已经导致摄像头 SCCB 失败。
6. 板上 `UART/TX/RX` 区域是 UART0 GPIO37/GPIO38，用于 USB 转串口和 JPEG/调试输出，不接 C6。
7. ESP32-P4 上不带 X-CUBE-AI 生成物；X-CUBE-AI 只用于 STM32 侧历史基线对照。
8. ESP32-P4 模型部署走 Espressif ESP-DL + ESP-PPQ，工程内最终部署 `.espdl`，不直接烧 ONNX。
9. 每次新增错误都写入本文档“错误记录”，并给出以后规则。

## 2. 当前确定引脚映射

完整表见 `docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx`。这里保留工程推进时最常看的版本。

| 模块 | 信号 | ESP32-P4 GPIO | 状态 | 说明 |
|---|---|---:|---|---|
| OV2640 | SCL | GPIO32 | 已验证固定 | 摄像头 SCCB，不能给语音 I2C |
| OV2640 | SDA | GPIO33 | 已验证固定 | 摄像头 SCCB，不能给语音 I2C |
| OV2640 | RESET/RST | GPIO16 | 已验证固定 | 保留 ST 成功时序 |
| OV2640 | PWDN | GPIO17 | 已验证固定 | 保留 ST 成功时序，最终保持低 |
| OV2640 | D0-D5 | GPIO18-GPIO23 | 已验证固定 | DVP 数据线 |
| OV2640 | D6 | GPIO26 | 已验证固定 | 不能当开发板 U1TX 使用 |
| OV2640 | D7 | GPIO27 | 已验证固定 | 不能当开发板 U1RX 使用 |
| OV2640 | PCLK | GPIO28 | 已验证固定 | DVP 像素时钟 |
| OV2640 | VSYNC | GPIO29 | 已验证固定 | DVP 场同步 |
| OV2640 | HREF/DE | GPIO30 | 已验证固定 | DVP 行有效 |
| OV2640 | XCLK | 不接 | 已验证固定 | ATK-MC2640 自带有源晶振，`CONFIG_EXAMPLE_DVP_XCLK_PIN=-1` |
| UART0 | TX/RX | GPIO37/GPIO38 | 已验证固定 | JPEG/调试输出，不接 C6 |
| 模型部署 | 无新增引脚 | - | 当前第一阶段 | 先只输出识别结果，不控制水泵 |
| 泵控 | PWM/EN | GPIO53 | 当前候选 | 默认上电停水，先断泵测试 |
| 水箱液位输入 | LEVEL | GPIO34 | 当前候选 | 需实测电平；异常优先停水 |
| 语音模块 | I2C SCL/SDA | GPIO46/GPIO47 | 当前候选 | 替代 GPIO32/GPIO33 |
| 按键 | KEY | GPIO45 | 当前候选 | 输入上拉，低有效，必须去抖 |
| ESP32-C6 | P4_TX/P4_RX | GPIO31/GPIO36 | 后续接入 | P4_TX GPIO31 -> C6_RX；P4_RX GPIO36 <- C6_TX |
| 身份识别模块 | UART + IO | GPIO48/GPIO49/GPIO50 | 后置候选 | AS608 不再强绑定，可换成成品人脸识别模块 |
| OLED | bit-bang | GPIO39-GPIO43 | 后置候选 | 仅在不用 SD 卡接口时使用 |
| DS18B20 | DQ | GPIO44 | 后置候选 | 1-Wire 单独验证 |
| 氛围灯 | LED | GPIO52/GPIO54 | 后置候选 | 后置普通输出 |

## 3. 禁用和冲突规则

| 引脚/接口 | 规则 |
|---|---|
| GPIO0-GPIO15 | 当前禁用，未重新供电并实测为 3.3V 前，不接外设 |
| GPIO26/GPIO27 | 已是摄像头 D6/D7，不能作为 C6 UART |
| GPIO32/GPIO33 | 已是摄像头 SCCB，不能接语音模块 |
| GPIO37/GPIO38 | UART0 调试/JPEG，不能接 C6 |
| 板上 `UART/TX/RX` 区域 | 对应 UART0，不是空闲串口 |

## 4. 工程迁移流程

### 阶段 0：摄像头基线

状态：已完成。

验收：

- OV2640 能连续输出 320x240 JPEG。
- 串口能解析完整 `FFD8` 到 `FFD9` 帧。
- 不再改摄像头引脚、XCLK、RESET/PWDN 时序。

### 阶段 1：模型部署

目标：把现有水位识别模型部署到 ESP32-P4，本阶段只输出识别结果，不控制水泵。

现有材料：

- 基线对照 INT8 ONNX：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\workspace\v3_w05_origline_plus_newdata_qat_st_pure_int8.onnx`
- ESP-PPQ 转换入口 float ONNX：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\workspace\v3_w05_origline_plus_newdata_qat_repacked_float.onnx`
- 预处理代码：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\preprocess_v3.py`
- 后处理和阈值：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\evaluate_onnx_sets.py`
- 基线记录：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\MODEL_BASELINES.md`
- 外部测试集：`C:\Users\heshizhe\Desktop\teste`
- 已处理测试集：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\processed_dataset_v3_selected_test_keep`
- 校准候选：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\workspace\calib_500_weighted_plus_hardset.npz`
- 当前 ESP-DL 导出目录：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\workspace\espdl_p4`
- 当前转换脚本：`C:\Users\heshizhe\Desktop\cnntry\water_ai_topview\tools\convert_to_espdl.py`

推荐路线：

1. 先在 PC 上检查模型输入尺寸、输入通道顺序、输出节点、opset、类别顺序和算子支持。
2. 用 `evaluate_onnx_sets.py` 在 `teste` 和已处理测试集上复核当前 ONNX 基线，目标对齐 `MODEL_BASELINES.md` 中的 `609/624 = 97.60%`。
3. 复刻 `preprocess_v3.py` 的预处理契约，生成 ESP-PPQ 可用的 calibration dataloader/input；不要假设 ESP-PPQ 直接吃现有 npz。
4. 使用 `espdl_quantize_onnx` 把 float ONNX 转成 ESP32-P4 专用 INT8 `.espdl`，同时保留 `.info` 和 `.json`；不要把带 `QuantizeLinear/DequantizeLinear` 的纯 INT8 ONNX 直接喂给 ESP-PPQ。
5. 在 PC 侧用 ESP-PPQ/ESP-DL 量化推理复核 `teste` 精度；如果明显低于 97.60%，先调整量化流程，不进 P4 工程。
6. 精度接近基线后，再把 `.espdl` 放入 ESP32-P4 工程，先跑静态输入，再接实时 OV2640。
7. 输出统一结果枚举：`NO_CUP`、`LOW_LEVEL`、`HALF_LEVEL`、`FULL_LEVEL`、`ABNORMAL`。

预处理契约：

- 输入相机画面按 320x240 处理。
- BGR/RGB 图像先转灰度。
- 320x240 补边到 320x320。
- 中心裁切 224x224。
- 在 224x224 上做中心圆形遮罩，半径 100。
- 输出 float32，范围 `[0, 1]`，等价于 `/255`。
- 训练/评估侧数组约定为单通道；现有校准 npz 为 `input (500, 1, 224, 224) float32`。

类别映射和泵控规则：

| 类别 | 含义 | 水泵规则 |
|---:|---|---|
| 0 | 无杯 | 正在出水时停 |
| 1 | 低水 | 继续 |
| 2 | 半杯 | 停 |
| 3 | 满杯 | 停 |
| 4 | 异常 | 停 |

后处理阈值：

- `low/half = 0.395`
- `half/full = 0.665`

上板技术路线：

- ESP-DL 部署模型需要 `.espdl` 格式；乐鑫官方文档说明 ESP-PPQ 可从 ONNX/PyTorch 量化并导出 ESP-DL 标准模型。
- 优先走 `espdl_quantize_onnx`。转换后必须保留三类文件：`*.espdl`、`*.info`、`*.json`。
- 已成功导出 `waterlevel_v3_from_qat_float_int8.espdl/.info/.json`；`.info` 显示输入为 `INT8, 1x224x224x1`，P4 端喂数前必须再次核对输入布局和量化指数。
- 当前 PC 侧 PPQ 复核结果：原始 500 样本校准集下，internal val 约 `454/486 = 93.42%`，raw argmax 约 `459/486 = 94.44%`；外部 `teste` 按板端后处理为 `551/624 = 88.30%`，raw argmax 为 `562/624 = 90.06%`。与 ST 外部基线 `609/624 = 97.60%` 差距过大，当前 `.espdl` 只能用于 P4 集成冒烟测试，不能作为最终可交付模型。
- 模型最终可用性以外部 `teste` 精度为主，内部集只作为过程参考；外部泛化掉精度明显时，先修量化/导出流程，不直接进入水泵闭环。
- 临时均衡训练集校准（每类 100 张）会把精度拉到不可用水平，不能替代当前校准集。
- ESP32-P4 上 Conv/GEMM 使用 per-channel 量化，其他算子多为 per-tensor；注意 ESP-DL、ESP-PPQ、ESP-IDF 版本匹配。
- 导出时保留 test input/output，便于在 P4 上做 test/profile。
- 若算子不支持，先改模型结构或替换算子，不在 P4 端硬写复杂算子。

验收：

- PC 端 float32、int8、espdl 仿真结果有对照表。
- P4 端静态输入结果与 PC 基准一致或差异可解释。
- P4 端实时摄像头输入能稳定输出结果、置信度、帧率和异常原因。

参考资料：

- ESP-DL 量化模型官方文档：https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html
- ESP-DL 加载、测试、profile 官方文档：https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_load_test_profile_model.html
- ESP-DL Getting Started：https://docs.espressif.com/projects/esp-dl/en/latest/getting_started/readme.html
- ESP-DL GitHub：https://github.com/espressif/esp-dl

### 阶段 2：摄像头与水泵联动

目标：模型结果进入统一状态机后再控制泵。

执行：

1. 新建硬件抽象：`pump_stop()`、`pump_set_pwm()`、`tank_level_read()`。
2. 泵默认关闭；上电、复位、摄像头无帧、模型未知、液位异常都停水。
3. 先断开泵负载，只打印泵命令。
4. 模型结果连续多帧稳定后才允许出水。
5. 满水、无杯、未知、无帧立即停水。

验收：

- 水泵动作只来自统一状态机。
- 识别函数不得直接控制泵。
- 任何异常路径都只会停水。

### 阶段 3：语音和按键

目标：恢复老人侧交互入口，但不绕过安全状态机。

执行：

1. 语音 I2C 使用 GPIO46/GPIO47。
2. 验证语音模块地址 `0x34`、识别结果寄存器 `0x64`、播报寄存器 `0x6E`。
3. 按键使用 GPIO45，做去抖、长按、连按保护。
4. 语音和按键只产生事件，不直接控制泵。
5. 播报走队列：普通提示不互相打断，安全异常可抢占但必须记录原因。

### 阶段 4：C6 串口接入

目标：接入已完成的 C6 联网代码。

接线：

- P4 TX GPIO31 -> C6 RX
- P4 RX GPIO36 <- C6 TX
- 波特率先用 115200

执行：

1. 先烧最小 UART 测试固件，确认 GPIO31 配置为 TX 后空闲为高。
2. P4 发 `P4_HELLO`，C6 回 `C6_ACK`。
3. 再迁移业务消息：设备状态、水位结果、出水开始、出水完成、异常停水。
4. C6 断联不影响 P4 本地安全停水。

### 阶段 5：其他模块

1. OLED 后置，必要时用 GPIO39-GPIO43，但必须确认不用 SD 卡接口。
2. 身份识别模块后置；AS608 不再强绑定，可换成成品人脸识别模块。
3. DS18B20、氛围灯后置。

## 5. 文件用途蓝图

### 5.1 顶层文件

| 文件 | 用途 |
|---|---|
| `.build-test-rules.yml` | ESP-IDF 示例工程的构建测试规则，通常不改 |
| `.gitignore` | 忽略 build、cache、临时抓图等文件 |
| `CMakeLists.txt` | ESP-IDF 顶层工程入口 |
| `dependencies.lock` | ESP-IDF 组件依赖锁定文件 |
| `README.md` | 原示例英文说明，保留参考 |
| `README_ZH.md` | 中文入口，指向本蓝图和 Excel |
| `VIBE_CODING_SETUP.md` | 本机 ESP-IDF / Vibe Coding 环境说明 |
| `sdkconfig` | 当前实际构建配置，已包含摄像头工作引脚 |
| `sdkconfig.defaults.esp32p4` | ESP32-P4 默认配置，改引脚时必须同步 |
| `sdkconfig.defaults*` | 其他芯片默认配置，非当前主线 |
| `sdkconfig.ci*` | 示例工程 CI 配置，非当前主线 |
| `sdkconfig.old` | ESP-IDF 生成的旧配置，已忽略，不作为依据 |
| `capture_clean.jpg` / `capture_test.jpg` | 本地调试抓图，已忽略，不作为源码 |

### 5.2 `main/`

| 文件 | 用途 |
|---|---|
| `main/capture_stream_main.c` | 当前主程序：OV2640 初始化、ST 风格 SCCB/RESET/PWDN、自检、V4L2 捕获、UART JPEG 输出 |
| `main/CMakeLists.txt` | main 组件构建入口 |
| `main/Kconfig.projbuild` | 示例配置项，包括 raw JPEG stream 等 |
| `main/idf_component.yml` | main 组件依赖声明 |

### 5.3 `components/example_video_common/`

| 文件/目录 | 用途 |
|---|---|
| `example_init_video.c` | ESP-Video 初始化、摄像头设备创建相关逻辑 |
| `example_encoder.c` | 示例编码器逻辑，当前不是主业务重点 |
| `example_storage.c` | 示例存储逻辑，当前不是主业务重点 |
| `include/example_video_common.h` | example video common 对外接口 |
| `include/boards/customized/example_video_common_board.h` | 当前 customized 板级配置来源之一 |
| `include/boards/*` | 乐鑫示例板配置，非当前主线 |
| `CMakeLists.txt`、`Kconfig.projbuild`、`idf_component.yml` | 组件构建与配置文件 |

### 5.4 `managed_components/`

这是 ESP-IDF 组件管理器拉下来的第三方依赖，文件很多，不逐个改。

| 目录 | 用途 |
|---|---|
| `espressif__esp_video` | V4L2/video device 相关框架 |
| `espressif__esp_cam_sensor` | 摄像头传感器驱动，当前 OV2640 寄存器表曾被调整 |
| `espressif__esp_new_jpeg` | JPEG 编解码相关库 |
| `espressif__usb_host_uvc` | UVC 示例/组件，当前不是主线 |
| `espressif__esp_h264` | H264 相关库，当前不是主线 |
| `espressif__cmake_utilities` | 构建辅助组件 |

规则：除非明确需要修组件 bug，否则不要在 `managed_components` 里大范围改动。已经改过的 OV2640 寄存器表必须在阶段提交中保留或迁移为补丁。

### 5.5 `scripts/`

| 文件 | 用途 |
|---|---|
| `scripts/idf-env.ps1` | 加载本机 ESP-IDF 环境 |
| `scripts/build.ps1` | 构建工程 |
| `scripts/flash.ps1` | 烧录工程 |
| `scripts/monitor.ps1` | 串口监视 |
| `scripts/flash-monitor.ps1` | 烧录后监视 |
| `scripts/full-clean.ps1` | 清理构建产物 |
| `scripts/activate-eim.ps1` | 激活 EIM/相关环境辅助脚本 |

### 5.6 `docs/`

| 文件 | 用途 |
|---|---|
| `docs/BLUEPRINT_RULE.md` | 唯一主蓝图和错误记录入口 |
| `docs/ESP32P4_CURRENT_PIN_MAPPING.xlsx` | 单独 Excel 引脚表 |

旧的分散 md 已删除，避免继续引用旧流程。

### 5.7 `patches/`

| 文件 | 用途 |
|---|---|
| `patches/esp-idf-v5.5.5-dvp-empty-jpeg-guard.patch` | 记录本机 ESP-IDF DVP 空 JPEG buffer 防崩溃补丁 |

### 5.8 生成/临时目录

| 目录 | 用途 | 规则 |
|---|---|---|
| `build/` | ESP-IDF 构建产物 | 不提交 |
| `.cache/` | 临时资料解压、分析缓存 | 不提交 |
| `tmp/` | 临时测试输出 | 不提交 |
| `.planning/` | 历史规划记录 | 可参考，但主线以本蓝图为准 |
| `.vscode/` | 本地编辑器配置 | 非主线逻辑 |

## 6. 错误记录

| 问题 | 现象 | 以后规则 |
|---|---|---|
| GPIO0-GPIO15 低压域误用 | SCL/SDA 空闲只有约 1.2-1.35V，OV2640 SCCB 失败 | 未重新供电并实测 3.3V 前不接外设 |
| 普通 I2C ACK 扫描误判 OV2640 | ACK 扫描失败但不等于 SCCB 不可用 | OV2640 按 ST 风格 SCCB 读 MID/PID |
| RESET/PWDN 随机试电平 | 偏离 ST 成功蓝图 | 保留 ST 成功时序 |
| 给 ATK-MC2640 输出 XCLK | 模块自带晶振，额外 XCLK 不必要 | `CONFIG_EXAMPLE_DVP_XCLK_PIN=-1` |
| 日志混入 JPEG | 上位机解析 JPEG 被污染 | raw JPEG 模式下关闭日志 |
| 未安装 UART driver 就发 JPEG | `uart_write_bytes()` 不可靠 | 原始 JPEG 输出前安装并配置 UART driver |
| ESP-IDF DVP 空 JPEG 崩溃 | 0 字节帧可能触发 load access fault | 保留 patch 记录并在环境中应用 |
| 把开发板 U1TX/U1RX 当 C6 串口 | GPIO26/GPIO27 已是摄像头 D6/D7 | C6 使用 GPIO31/GPIO36 |
| 把板上 UART/TX/RX 接 C6 | 那一路是 UART0/JPEG/USB 转串口 | C6 不接 UART0 |
| 语音接开发板 IIC 口 | GPIO32/GPIO33 已是摄像头 SCCB | 语音 I2C 使用 GPIO46/GPIO47 |
| 在 ESP32-P4 工程带入 X-CUBE-AI 生成物 | X-CUBE-AI 只适配 STM32 工作流，P4 不能直接用 | P4 只部署 ESP-DL `.espdl` 模型 |
| 直接把 ONNX 放进 P4 工程 | ESP-DL 官方部署格式是 `.espdl` | ONNX 先经 ESP-PPQ 转换，保留 `.espdl/.info/.json` |
| 忽略预处理差异 | ROI/灰度/224x224/遮罩/量化不一致会直接掉精度 | P4 必须复刻 `preprocess_v3.py` |
| 只看内部验证集就判断模型可部署 | PPQ internal val 为 `93.42%`，但外部 `teste` 只有 `88.30%`；泛化掉精度后水位模型不可直接用于闭环 | 模型验收最终看外部 `teste`，内部集只作过程参考 |
| 直接用纯 INT8 Q/DQ ONNX 做 ESP-PPQ 输入 | `QuantizeLinear` 在 PPQ 追踪阶段无 FP32 backend，转换失败 | 纯 INT8 ONNX 只作基线对照；ESP-PPQ 入口使用同结构 float ONNX，再导出 ESP32-P4 INT8 `.espdl` |
| 用均衡训练集替换当前 500 样本校准集 | PPQ internal val 退化到约 `82/486 = 16.87%` | 暂不改校准分布；当前最稳仍是 `calib_500_weighted_plus_hardset.npz` |
| 把 16-bit 当成简单提精度捷径 | 16-bit 全量评估过慢；100-step smoke 受校准截断影响，结论不可靠 | 先把 8-bit 主线跑稳，再考虑 16-bit |

新增错误模板：

```text
### YYYY-MM-DD 问题标题
- 现象：
- 已排除：
- 根因：
- 最终处理：
- 以后规则：
```

## 7. 阶段性提交规则

只在以下阶段达成后提交：

1. `camera baseline`
2. `model inference on p4`
3. `camera pump closed loop`
4. `voice and button control`
5. `p4 c6 uart integration`
6. `optional modules integration`
7. `complete dispenser workflow`

提交前检查：

- 摄像头引脚没有被改。
- `GPIO0-GPIO15` 没有被新模块使用。
- Excel 引脚表与代码一致。
- 本蓝图已记录新错误和新规则。
