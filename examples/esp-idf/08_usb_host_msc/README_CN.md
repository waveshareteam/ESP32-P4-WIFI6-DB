| 支持目标 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| -------- | -------- | -------- | -------- | -------- |

# USB 大容量存储类示例

[English Version](./README.md)

## 概述

本示例演示如何使用 MSC（Mass Storage Class）访问 U 盘上的存储。U 盘插入后会被挂载到虚拟文件系统，然后依次执行下列操作：

1. 打印设备信息（容量、扇区大小和扇区数量等）。
2. 列出 U 盘根目录下的所有文件夹和文件。
3. 创建 `esp` 子目录（如果尚不存在）以及一个 `test.txt` 文件。
4. 通过向 `dummy` 文件传输 1 MB 数据来运行读写性能测试。

> 注意：本示例目前只支持 FAT 格式的驱动器。exFAT、NTFS 等其他文件系统与本示例不兼容。请确认 U 盘为 FAT 格式，以避免兼容性问题。

### USB 重新连接

示例在循环中运行，以便演示 USB 连接和重新连接的处理。如果需要反初始化整个 USB Host 协议栈，可以把 GPIO0 短接到 GND。GPIO0 通常映射为 BOOT 按键，因此按下该按键即可反初始化协议栈。

### 所需硬件

* 支持 USB-OTG 的开发板
* 一根用于供电和烧录的 USB 线
* 一个 U 盘

### USB Host 限制

#### ESP32-S2 和 ESP32-S3
ESP32-S2 和 ESP32-S3 的 USB OTG 外设在主机模式下最多支持 **8 个双向端点**，每个端点都可以配置为 IN 或 OUT。

由于每个 USB 大容量存储类（MSC）设备通常需要 **3 个端点**（控制、BULK IN 和 BULK OUT），而 USB 集线器同样会占用端点（控制、INTERRUPT IN），因此理论上最多只能连接 **2 个** MSC 设备。

#### ESP32-P4
ESP32-P4 采用更先进的 USB 2.0 OTG 控制器，主机模式下最多支持 **16 个双向端点**。在每个设备端点需求相同的前提下，理论上最多可连接 **4 个** MSC 设备。

### 示例限制
同时连接的 MSC 设备数量现在由 **`CONFIG_FATFS_VOLUME_COUNT`** 决定。这样可以与 FAT 文件系统的配置保持一致，避免 MSC 设备数量和可用 FAT 卷数量不匹配。

`CONFIG_FATFS_VOLUME_COUNT` 默认值为 **2**，即最多可同时连接 **2 个 MSC 设备**。如果应用需要更多设备，可以相应增大该值，但要注意**端点和内存的限制**。

#### 如何在 menuconfig 中配置 `CONFIG_FATFS_VOLUME_COUNT`：
要修改 MSC 设备的最大数量，在 **menuconfig** 中调整 `CONFIG_FATFS_VOLUME_COUNT`：
idf.py menuconfig → Component config → FAT Filesystem support → Number of FATFS volumes
把该值设为期望的 MSC 设备数量，并确保与可用的 USB 端点数量相匹配。

### 常用引脚分配

具体的硬件连接方式请参考仓库根目录的 [README](../../../README_CN.md)。

此外，把 GPIO0 短接到地可以反初始化 USB 协议栈。

### 构建和烧录

构建工程并烧录到开发板，然后运行监视工具查看串口输出：

```
idf.py -p PORT flash monitor
```

（退出串口监视器请按 ``Ctrl-]``。）

配置和使用 ESP-IDF 构建工程的完整步骤请参考 Getting Started Guide。

## 示例输出

```
...
I (323) example: Waiting for USB flash drive to be connected
I (3353) example: MSC device connected (usb_addr=3)
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 2.00
bDeviceClass 0x0
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 64
idVendor 0xabcd
idProduct 0x1234
bcdDevice 1.00
iManufacturer 1
iProduct 2
iSerialNumber 3
bNumConfigurations 1
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 32
bNumInterfaces 1
bConfigurationValue 1
iConfiguration 0
bmAttributes 0x80
bMaxPower 100mA
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 2
        bInterfaceClass 0x8
        bInterfaceSubClass 0x6
        bInterfaceProtocol 0x50
        iInterface 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x1    EP 1 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 512
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 512
                bInterval 0
Device info:
         Capacity: 3839 MB
         Sector size: 512
         Sector count: 7864319
         PID: 0x1234
         VID: 0xABCD
         iProduct: UDisk
         iManufacturer: General
         iSerialNumber:
I (3763) example: ls command output for all connected devices:
I (3763) example: Listing contents of /usb0
/usb0/SYSTEM~1
/usb0/ESP
I (3773) example: Reading file
I (3773) example: Read from file '/usb0/esp/test.txt': 'Hello World!'
I (3803) example: Writing to file /usb0/esp/dummy
I (3943) example: Write speed 7.16 MiB/s
I (3953) example: Reading from file /usb0/esp/dummy
I (4093) example: Read speed 7.16 MiB/s
I (4103) example: Example finished, you can disconnect the USB flash drive (or connect another USB flash drive)
```
