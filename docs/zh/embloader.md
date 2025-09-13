# 基于UEFI多种板子的通用Linux引导加载器解决方案

## 文档版本

- 2025/09/04 v0.1 初稿版本
- 2025/09/09 v0.1 embloader初步实现demo
- 2025/09/12 v0.1 embloader第一版本

## 简介

该文档描述一种新的引导程序，用于统一多平台的启动配置，并且实现dtbo加载

该引导程序可以内置在UEFI固件(U-Boot, EDK II)最先启动，也可以放在ESP中作为默认引导器

~~最理想的情况下，使用该引导程序的镜像将只区分CPU架构平台(ARM64, X86_64, RISCV64),
所有的板子将可以使用同一个镜像(如果U-Boot或EDK II放在板载的SPI Flash中)~~

> 目前可以先做到同一平台使用统一的镜像(因为不同平台分区表不一样等)。如全志平台共用一个镜像，瑞芯微共用一个镜像，高通共用一个镜像等。
>
> 后续通过安装器可以做到按CPU架构区分镜像(如nanodistro)。

## 适用平台

- 瑞芯微 / Rockchip, 全志 / Allwinner

  UEFI实现: U-Boot EFI

- 高通 / Qualcomm

  UEFI实现: EDK II

(其它未列出的UEFI Bootloader平台也可以支持)

## 启动流程

1. 引导程序读取`embloader/config.static.yaml`文件

   使用正则表达式匹配选择板子(如通过SMBIOS信息)，板子信息包括默认的设备树和dtbo文件夹，默认主题等

   该文件保持静态不变

2. 引导程序读取`embloader/config.dynamic.yaml`文件

   加载用户配置，例如要启用的一些dtbo和个性化配置等
    
   该文件可以被配置程序(如系统内的可视化配置工具)修改，也可以被此引导程序修改

4. 加载默认dtb和dtbo

   加载前面配置的dtb，并附加上选定的dtbo

5. 加载启动项并渲染菜单

- 方式1: `embloader/config.static.yaml` 或者 `embloader/config.dynamic.yaml` 定义的启动项

  使用前面菜单配置的启动项

  当使用了多种启动配置方式的时候在前面添加`(Config)`字样

- 方式1: Boot Loader Specification

  使用UAPI定义的引导加载器规范，与 systemd-boot 兼容

  https://uapi-group.org/specifications/specs/boot_loader_specification/

  加载 /loader/loader.conf 作为启动菜单的基础配置

  加载 /loader/entires/*.conf 作为启动项列表

  当使用了多种启动配置方式的时候在前面添加`(UAPI)`字样

- 方式2: extlinux (未实现)

  使用传统的extlinux.conf配置

  https://wiki.syslinux.org/wiki/index.php?title=EXTLINUX

  加载 /extlinux/extlinux.conf 作为启动菜单的基础配置和启动项

  当使用了多种启动配置方式的时候在前面添加`(ExtLinux)`字样

- 方式3: 加载下一阶段引导器

  如果前面的配置文件没有找到合适可用的配置文件，则转交控制权给接下来的引导器(比如用户安装的GRUB2)

  接下来要使用的引导器由EFI实现选择，该引导器只是简单的退出。如EFI Variables(变量)，或者`/EFI/BOOT/BOOTAA64.EFI`

  由于前面已经有选择好的dtb，所以接下来的引导器可以不处理dtb

## 配置文件示例

### `embloader/config.static.yaml`

```yaml
devices:
- name: Radxa ROCK 5B
  match:

  # 'oper' 项可用的值
  # - equals          完全等于
  # - noteq           不等于
  # - contains        字符串包含
  # - regexp          正则表达式匹配
  # - greater         数字或ASCII比较 field版本大于value版本
  # - less            数字或ASCII比较 field版本小于value版本
  # - greater-equals  数字或ASCII比较 field版本大于等于value版本
  # - less-equals     数字或ASCII比较 field版本小于等于value版本
  # - older           版本比较 field版本低于value版本
  # - newer           版本比较 field版本高于value版本

  # 完全匹配厂家名
  - field: smbios.type2.manufacturer # SMBIOS -> 主板信息 -> 厂家名称
    oper: equals
    value: Radxa

  # 完全匹配设备名
  - field: smbios.type2.product # SMBIOS -> 主板信息 -> 设备名
    oper: equals
    value: ROCK 5B

  # (可选) 匹配设备版本
  - field: smbios.type2.version # SMBIOS -> 主板信息 -> 版本
  - oper:  newer
  - value: '1.2'

  # 该设备支持的配置
  profiles:
  - common
  - rockchip
  - rk3588
  - radxa
  - radxa-rock5b

- name: Radxa Dragon Q6A
  match:
  - field: smbios.type2.manufacturer
    oper: equals
    value: Radxa
  - field: smbios.type2.product
    oper: equals
    value: Dragon Q6A

  profiles:
  - common
  - qualcomm
  - qcs6490
  - radxa
  - radxa-q6a

profiles:
  rk3588:
    # 添加到启动参数 (仅支持YAML启动项)
    bootargs:
    - console=ttyS0
  radxa-rock5b:
    # 后续设备树选择的ID
    dtb-id: rk3588-radxa-rock5b
    # 默认的ktype, 仅在引导选项中没有设置ktype时使用
    ktype: mainline
    bootargs:
    - example.device=rock5b
  radxa-q6a:
    dtb-id: qcs6490-radxa-q6a
    bootargs:
    - example.device=dragon-q6a

# 引导菜单
menu:
  timeout: 30
  default: ubuntu-24.04
  save-default: yes

# 引导选项
loaders:
  efisetup:
    title: Reboot into Firmware Setup
    type: efisetup
    priority: 1000
     # 如果YAML配置文件中的所有启动项都是complete==no，则视为YAML没有可用的
    complete: no
    match:
    # 如果使用U-Boot实现的EFI则隐藏该选项
    - field: smbios.type0.vendor
      oper: noteq
      value: U-Boot
  poweroff:
    title: Power Off
    type: reboot
    priority: 1001
    mode: poweroff
    complete: no
  reboot:
    title: Reboot
    priority: 1002
    type: reboot
    complete: no

# 设备树和Overlay相关
devicetree:
  # 使用的dtbo文件夹
  # ktype由启动项指定; profile则扫描已支持的配置确定
  dtbo-dir: /dtbo/{ktype}/{profile}/
  # 默认将使用的dtb文件
  default-dtb: /dtbs/{ktype}/{profile}/${dtb-id}.dtb
```

### `embloader/config.dynamic.yaml`

```yaml
devicetree:
  overlays:
    # 示例设备: I2C转多通道PWM的外设
    # 使用Q6A和ArchLinuxARM主线: /dtbo/mainline/common/pca9685.dtbo
    pca9685:
      enabled: yes
      params:
        i2c: i2c0
    # 示例设备: HDMI转CSI小板
    # 使用ROCK 5B和Ubuntu BSP内核: /dtbo/rk3588-6.1/common/rk628d.dtbo
    rk628d:
      enabled: yes
      params:
        i2c: i2c2
        csi: csi0
        lanes: 2
        i2s: i2s1
    # 特定平台的引脚复用配置
    # 使用ROCK 5B和Ubuntu BSP内核: /dtbo/rk3588-6.1/rk3588/i2c2m1.dtbo
    i2c2m1:
      enabled: yes

# 引导选项
loaders:
  archlinuxarm:
    title: Archlinux ARM with Universal Mainline Kernel
    type: linux-efi
    kernel: /vmlinuz-linux-mainline
    initramfs:
    - /initramfs-linux-mainline.img
    bootargs:
    - loglevel=7
    - panic=30
    # 内核类型 用于不同内核版本选择dtbo或者dtb
    ktype: mainline
  ubuntu-2404:
    title: Ubuntu 24.04 with Ubuntu Kernel
    type: linux-efi
    kernel: /vmlinuz-ubuntu
    profiles:
      # 不在选定的配置上显示
      disallowed: 
      - sun55i # 假设ubuntu的内核在sun55i平台无法使用
    initramfs:
    - /initrd-ubuntu.img
    bootargs:
    - loglevel=7
    - panic=30
    ktype: ubuntu-2404
  android-15-rk3588:
    title: Android 15 with RK3588 Vendor kernel
    profiles:
      # 仅在选定的配置上显示
      allowed:
      - rk3588
    type: abootimg # 安卓启动镜像
    boot: /boot-android15.img
    ktype: rk3588-6.1
```

### `embloader/config.dynamic.yaml`

```conf
## example file
devicetree.overlays.qcs6490-kvm.enabled = true
test.value = "string value"
test.array[0].val0 = 0.0
menu.timeout = 30
```

### `embloader/dtbo.txt`

```conf
# 普通的dtbo
dtoverlay=ramoops
# 向dtbo传递参数
dtoverlay=pca9685,i2c=i2c0
# 仅对具有特定profile的设备生效
dtoverlay.qcs6490=qcs6490-kvm
```

### `embloader/cmdline.txt`

```conf
## 支持注释
# 单行多个参数
ro root=/dev/sda2
# 每行一个参数
console=ttyS0
console=tty1
```

## 项目基础信息

- 名称使用embloader (Embedded Bootloader)
- GPL v2.0协议开源 (暂定)
- 使用C语言编写，构建系统EDK II
- 首要支持架构ARM64，后续扩展支持RISC-V 64、x86_64、ARM32

## 路线图

- [x] 项目基础架构构建
- [x] 按设备信息选择dtb的Demo
- [x] 完善多个设备的SMBIOS，并通过EFI启动(U-Boot/EDK II)
- [x] dtbo支持
- [ ] 启动菜单多启动项支持
- [ ] 配置工具增强
  - [ ] 系统内TUI版本的配置工具 (暂定Python)
  - [ ] 系统内GUI版本的配置工具 (暂定Python)
  - [ ] UEFI环境下的TUI/GUI配置工具
- [ ] 统合安装器

(待补充)
