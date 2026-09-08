# ESP32-C5 Hosted Wi-Fi Station / SoftAP

[English Version](./README.md)

本示例通过 ESP32-C5 无线协处理器和 ESP-Hosted 验证 ESP32-P4 的 Wi-Fi
功能。默认运行 Station 模式，也可以切换到 SoftAP 模式，供手机连接和信号检查。

## 硬件要求

- ESP32-P4 与 ESP32-C5 已按开发板定义连接 ESP-Hosted 传输总线。
- ESP32-C5 已单独烧录与主机侧版本匹配的 ESP-Hosted 协处理器固件。
- Station 模式需要 2.4 GHz 接入点；SoftAP 模式需要手机或其他 Wi-Fi 客户端。

ESP32-P4 本身没有原生 Wi-Fi 射频。烧录本主机工程不会更新 ESP32-C5
固件；主机与协处理器的 ESP-Hosted 版本、传输引脚和复位接线必须匹配。

## 默认传输配置

`sdkconfig.defaults` 已选择：

```text
CONFIG_SLAVE_IDF_TARGET_ESP32C5=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
```

组件 manifest 会根据 ESP-IDF 版本解析对应的 `esp_wifi_remote` 和
`esp_hosted` 版本。

## 构建和烧录

请在 ESP-IDF PowerShell 中执行：

```powershell
cd examples\esp_idf\06_wifi
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为 ESP32-P4 的实际串口号。

## 配置

在 `menuconfig` 中打开 **Example Configuration**：

- 设置 **WiFi SSID** 和 **WiFi Password**，两种模式共用这两个字段。
- 保持 **Run in SoftAP mode** 关闭时运行 Station 模式。
- 打开该选项后运行 SoftAP 模式，并可设置信道和最大连接数。
- Station 模式下可按需要调整最大重试次数和最低认证模式。

源码会打印配置的 SSID 和密码，不要在准备分享的串口日志中使用生产环境凭据。

## 预期行为

### Station 模式

串口应显示 ESP-Hosted 启动、必要的连接重试、DHCP 地址和网关 Ping 结果。

### SoftAP 模式

串口应包含：

```text
SoftAP ready
SSID: myssid
Password: mypassword
Channel: 1, max connections: 4
AP IP address: 192.168.4.1
Station connected: XX:XX:XX:XX:XX:XX, aid=1
```

## 排障

- 如果 ESP-Hosted 无法连接，先检查 ESP32-C5 固件、传输引脚、复位 GPIO
  和协处理器目标选择，再排查 Wi-Fi。
- 确认接入点或手机使用 2.4 GHz Wi-Fi。
- Station 模式检查 SSID、密码和认证门限。
- SoftAP 模式使用空密码创建开放网络，或使用不少于 8 个字符的 WPA2 密码。
- 编辑器内托管组件解析失败时，改用 ESP-IDF 命令行环境重试。

以上内容已按源码和配置做静态核对；工程编译、烧录、Station 连接、SoftAP
连接和射频表现仍为待实测。
