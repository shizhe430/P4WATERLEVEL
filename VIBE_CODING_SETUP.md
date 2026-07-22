# ESP32-P4 Vibe Coding 工程说明

本工程独立位于 `C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view`，不依赖 STM32 工程目录。当前环境采用乐鑫官方 ESP-IDF v5.5.5，目标芯片为 ESP32-P4。

## 已完成的环境配置

- Cursor 已安装官方 `Espressif IDF` 扩展 `espressif.esp-idf-extension` v2.1.0。
- 工具链位于 `C:\Espressif`，Python 环境为 Python 3.11.2。
- 工程内 `.vscode/settings.json` 已绑定 EIM 元数据、ESP-IDF 路径、目标芯片、Flash 和 Monitor 波特率。
- 工程内 `.vscode/tasks.json` 提供 Build、Full Clean、Flash、Monitor、Build Flash Monitor 任务。
- `scripts` 目录中的脚本会自动初始化官方 ESP-IDF 环境，不需要手动配置系统 PATH。

## 在 Cursor 中使用

1. 用 Cursor 打开整个目录：

   `C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view`

2. 第一次打开时确认安装推荐扩展 `Espressif IDF`。

3. 底部状态栏选择 ESP32-P4，并确认串口为开发板实际出现的 `COMx`。没有连接开发板时串口会显示为自动检测，这是正常的。

4. 使用 ESP-IDF 扩展状态栏中的 Build、Flash Device、Monitor Device 或 Build, Flash and Monitor。也可以运行工程任务：`Terminal` -> `Run Task`。

## 在 PowerShell 中使用

```powershell
Set-Location C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view
& .\scripts\build.ps1
& .\scripts\flash.ps1 -Port COM7
& .\scripts\monitor.ps1 -Port COM7
```

如果不指定 `-Port`，ESP-IDF 会尝试自动检测串口。烧录使用 460800 baud，监视器使用 921600 baud；这与工程的 UART0 配置一致。

## 摄像头验证

当前工程默认使用诊断输出模式，避免 JPEG 二进制流与日志混在同一个串口中。上电后通过 Monitor 检查 OV2640 PID、初始化结果和持续帧计数。

确认摄像头稳定后，在 `idf.py menuconfig` 的 `Example Configuration` 中打开原始 JPEG 输出选项，再重新 Build、Flash。该模式会把完整 JPEG 帧通过 UART0 输出，接收端应按 `FF D8` 和 `FF D9` 分帧。

## 物理烧录前检查

- DNESP32P4M 与摄像头模块必须共地，数字信号必须为 3.3 V。
- 摄像头 XCLK、SCCB、D0-D7、PCLK、VSYNC、HREF/DE 的连线必须与 `sdkconfig.defaults.esp32p4` 一致。
- 连接开发板后执行 `Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description` 确认 COM 口。
- 当前未检测到实际 COM 口，因此尚未宣称物理烧录成功；Build 已可在本机验证，Flash 需要连接硬件后执行。

## 重要路径

- ESP-IDF：`C:\Espressif\frameworks\esp-idf-v5.5.5`
- EIM 元数据：`C:\Espressif\tools\eim_idf.json`
- EIM CLI：`C:\Espressif\tools\eim\eim.exe`
- 本项目：`C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view`
