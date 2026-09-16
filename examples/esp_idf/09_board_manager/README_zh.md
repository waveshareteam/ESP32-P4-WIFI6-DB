| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Board Manager 测试应用

[English](README.md)

一个交互式串口控制台，通过 [ESP Board Manager](https://github.com/espressif/esp-board-manager)
初始化 ESP32-P4-WIFI6-DB 开发板，并在真实硬件上运行功能测试用例
（LCD、触摸、音频、摄像头、SD 卡、GPIO 等）。板卡定义位于
[`components/waveshare_bmgr`](../../../components/waveshare_bmgr)。

本工程改编自 Espressif 官方
[esp-board-manager](https://github.com/espressif/esp-board-manager) 组件自带的
`test_apps` 工程（v0.7.x，首次编译后可在
`managed_components/espressif__esp_board_manager/test_apps` 中找到），裁剪为
仅面向 ESP32-P4，并指向 Waveshare 板卡包。测试框架、各设备/外设测试源码、
分区表和 `sdkconfig.defaults*` 保留了 Espressif 原有的
`LicenseRef-Espressif-Modified-MIT` 文件头，完整许可证文本见本目录下的
[`LICENSE`](LICENSE)，与本仓库其余部分使用的 Apache-2.0 许可证相互独立。

## 1. Board Manager 环境准备

`idf.py bmgr` 命令由 `esp_board_manager` 组件提供，不是 ESP-IDF 自带的。
在 ESP-IDF 的 Python 环境中（先用 `export.ps1` / `export.sh` 激活）安装一次
`esp-bmgr-assist`，它会自动为所有工程注册 `bmgr` 命令：

```bash
pip install esp-bmgr-assist
```

验证是否生效：

```bash
idf.py bmgr -l -c ../../../components/waveshare_bmgr
```

列表中应能看到 `esp32_p4_wifi6_db`。

## 2. 编译与烧录

以下命令都在本目录下执行。

```bash
idf.py set-target esp32p4
```

生成板卡配置。`-c` 指向本仓库的板卡包（它没有发布到组件注册表，不会被
自动发现）：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c ../../../components/waveshare_bmgr
```

默认屏幕是 10.1 寸 DSI 屏。如果使用其他 Waveshare DSI 触摸屏，加上 `-a`
指向对应的 amend 目录（`lcd_5_dsi_touch_a`、`lcd_7_dsi_touch_a`、
`lcd_8_dsi_touch_a`、`lcd_10_1_dsi_touch_a`）：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c ../../../components/waveshare_bmgr -a ../../../components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

然后编译、烧录并打开串口监视器：

```bash
idf.py build flash monitor
```

说明：

- `bmgr` 会生成 `components/gen_bmgr_codes/`（已被 git 忽略）并删除
  `build/CMakeCache.txt`，下一次 `build` 会自动重新配置。切换屏幕 amend 或
  修改板卡 YAML 后需要重新执行 `bmgr`；`idf.py bmgr -x` 可清除已生成的配置。
- `set-target` 可以在 `bmgr` 之前执行，但 `build` 之前必须至少成功执行过
  一次 `bmgr`。
- 控制台默认走 UART0（USB 下载口）。如果只接了 USB-Serial/JTAG 口，在
  `idf.py menuconfig` → *Component config → ESP System Settings → Channel
  for console output* 中切换控制台通道。

## 3. 运行后的使用方式

启动后监视器里会出现命令提示符，输入 `help` 可查看全部命令。主要用到
`bmgr` 和 `case` 两个命令：

```
bmgr init                  # 初始化板上所有设备和外设
bmgr init lcd_touch        # 按名称初始化单个设备/外设
bmgr info                  # 打印板卡名称 / 芯片 / 版本
bmgr print                 # 打印全部外设和设备配置
bmgr status                # 显示当前已初始化的设备
bmgr deinit                # 释放全部（或 `bmgr deinit <name>`）

case list                  # 列出本板可用的测试用例
case list --group lcd      # 只列出某一组
case run lcd.color.red     # 运行单个用例
case run-all --group periph  # 运行某一组的全部用例
case run-all               # 运行全部非手动用例
case run-all --include-manual  # 连同需要人工参与的用例一起运行
```

典型流程：

```
bmgr init
case list
case run lcd.pattern.checker
case run periph.i2c_probe
bmgr deinit
```

用例分为 `audio`、`button`、`camera`、`custom`、`fs`、`gpio_expander`、
`knob`、`lcd`、`led`、`periph` 几组。只有板卡实际声明了对应设备/外设的
用例才会被编译进来，所以 `case list` 显示的就是本板能跑的全部用例。

在 `case` 命令后加 `--summary`（可读格式）或 `--json`（机器可读格式）可在
运行结束时输出通过/失败统计。

测试用例按 Board Manager 的惯用名称查找设备。本板唯一的 `custom` 设备是
`lcd_brightness`（I2C 背光控制器），`sdkconfig.defaults` 已通过
`CONFIG_BMGR_TEST_NAME_CUSTOM` 把 `custom.basic` 指向它。其他名称可在
`idf.py menuconfig` → *Board Manager Test App → Device/Peripheral name
overrides* 中覆盖。
