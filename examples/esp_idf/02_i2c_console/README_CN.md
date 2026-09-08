| 支持目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| -------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- |

# I2C Tools 示例

[English Version](./README.md)

## 概述

[I2C Tools](https://i2c.wiki.kernel.org/index.php/I2C_Tools) 是开发 I2C 相关应用时非常实用的一个简单工具，在 Linux 平台上也很有名。本示例基于 [esp32 console 组件](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/console.html) 实现了 [I2C Tools](https://i2c.wiki.kernel.org/index.php/I2C_Tools) 的部分基本功能。如下所示，本示例支持五个命令行工具：

1. `i2cconfig`：使用指定的 GPIO 编号和频率配置 I2C 总线。
2. `i2cdetect`：扫描 I2C 总线上的设备，并输出一张列出已检测到设备的表格。
3. `i2cget`：读取 I2C 总线上可见的寄存器。
4. `i2cset`：设置 I2C 总线上可见的寄存器。
5. `i2cdump`：检视 I2C 总线上可见的寄存器。

如果你在开发 I2C 相关应用时遇到问题，或者只是想测试某个 I2C 设备的部分功能，可以先用这个示例试一试。

## 如何使用示例

### 硬件要求

运行本示例需要一块基于 ESP32、ESP32-S、ESP32-C、ESP32-H 或 ESP32-P 的开发板。为了便于测试，还需要一个带 I2C 接口的设备。这里以 CCS811 传感器为例，演示如何在不写任何代码的情况下（只使用本示例支持的命令行工具）测试该传感器的功能。关于 CCS811 的更多信息，可以查阅[在线数据手册](http://ams.com/ccs811)。

#### 引脚分配：

**注意：** 必须先运行 `i2cconfig` 命令，用正确的 GPIO 建立 I2C 总线。
**注意：** 建议为 SDA/SCL 引脚外接上拉电阻使通信更稳定，尽管驱动会启用内部上拉电阻。

### 构建和烧录

运行 `idf.py -p PORT flash monitor` 构建工程并烧录到开发板。

（退出串口监视器请按 ``Ctrl-]``。）

配置和使用 ESP-IDF 构建工程的完整步骤，请参考 [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)。

## 示例输出

### 查看所有支持的命令及其用法

```bash
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
i2c-tools> help
help  [<string>] [-v <0|1>]
  Print the summary of all registered commands if no arguments are given,
  otherwise print summary of given command.
      <string>  Name of command
  -v, --verbose=<0|1>  If specified, list console commands with given verbose level

i2cconfig  --scl=<gpio> --sda=<gpio> [--freq=<Hz>]
  Config I2C bus frequency and IOs
  --scl=<gpio>  Set the gpio for I2C SCL
  --sda=<gpio>  Set the gpio for I2C SDA
  --freq=<Hz>  Set the frequency(Hz) of I2C bus

i2cdetect
  Scan I2C bus for devices

i2cget  -c <chip_addr> [-r <register_addr>] [-l <length>]
  Read registers visible through the I2C bus
  -c, --chip=<chip_addr>  Specify the address of the chip on that bus
  -r, --register=<register_addr>  Specify the address on that chip to read from
  -l, --length=<length>  Specify the length to read from that data address

i2cset  -c <chip_addr> [-r <register_addr>] [<data>]...
  Set registers visible through the I2C bus
  -c, --chip=<chip_addr>  Specify the address of the chip on that bus
  -r, --register=<register_addr>  Specify the address on that chip to read from
        <data>  Specify the data to write to that data address

i2cdump  -c <chip_addr> [-s <size>]
  Examine registers visible through the I2C bus
  -c, --chip=<chip_addr>  Specify the address of the chip on that bus
  -s, --size=<size>  Specify the size of each read
```

### 配置 I2C 总线

> [!IMPORTANT]
> 使用其他 I2C 命令之前，必须先运行 `i2cconfig` 命令。

```bash
i2c-tools> i2cconfig --sda=7 --scl=8 --freq=100000
```

* `--sda` 和 `--scl` 选项用于指定 I2C 总线使用的 GPIO 编号，这里选择 GPIO7 作为 SDA、GPIO8 作为 SCL，与 ESP32-P4-WIFI6-DB 的共享 I2C1 总线一致。
* `--freq` 选项用于指定 I2C 总线频率，这里设置为 100KHz。

### 查看 I2C 总线上的 I2C 地址（7 位）

```bash
i2c-tools> i2cdetect
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- 5b -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
```

* 这里我们找到 CCS811 的地址是 0x5b。

### 读取状态寄存器的值

```bash
i2c-tools> i2cget -c 0x5b -r 0x00 -l 1
0x10
```

* `-c` 选项用于指定 I2C 设备的地址（由 `i2cdetect` 命令获得）。
* `-r` 选项用于指定要查看的寄存器地址。
* `-l` 选项用于指定读取内容的长度。
* 这里返回值 0x10 表示传感器刚处于 boot 模式，已经准备好进入 application 模式。关于 CCS811 的更多信息请查阅[官方网站](http://ams.com/ccs811)。

### 切换工作模式

```bash
i2c-tools> i2cset -c 0x5b -r 0xF4
I (734717) cmd_i2ctools: Write OK
i2c-tools> i2cset -c 0x5b -r 0x01 0x10
I (1072047) cmd_i2ctools: Write OK
i2c-tools> i2cget -c 0x5b -r 0x00 -l 1
0x98
```

* 这里我们把模式从 boot 切换到 application，并设置了合适的测量模式（向寄存器 0x01 写入 0x10）。
* 此时传感器的状态值为 0x98，表示已有有效数据可以读取。

### 读取传感器数据

```bash
i2c-tools> i2cget -c 0x5b -r 0x02 -l 8
0x01 0xb0 0x00 0x04 0x98 0x00 0x19 0x8f
```

* 寄存器 0x02 会输出 8 字节结果，主要包括 eCO~2~、TVOC 的值以及它们的原始值。因此 eCO~2~ 的值为 0x01b0 ppm，TVOC 的值为 0x04 ppb。

## 排障

* 运行 `i2cdetect` 命令时找不到任何可用地址。
  * 确认接线连接正确。
  * 有些传感器带有 “wake up” 引脚，用户可以通过它让传感器进入睡眠模式。请确认传感器**不是**处于睡眠状态。
  * 复位 I2C 设备，然后重新运行 `i2cdetect`。
* 运行 `i2cdump` 命令时得不到正确的内容。
  * 目前 `i2cdump` 只支持 I2C 设备内部各寄存器内容长度相同的情况。例如某个设备有三个寄存器地址，这些地址上的内容长度分别是 1 字节、2 字节和 4 字节，这种情况下不应期望该命令能正确 dump 出寄存器内容。
