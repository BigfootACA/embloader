# embloader - Embedded Bootloader

A modern, multi-platform UEFI bootloader designed to unify Linux boot configurations across different ARM and x86 platforms with advanced device tree overlay support.

## Overview

Embloader is a sophisticated boot loader that provides a unified boot experience across multiple hardware platforms. It features automatic device detection, dynamic device tree overlay loading, and systemd-boot compatibility, making it ideal for embedded systems and development boards.

### Key Features

- **Universal Platform Support** - Works with U-Boot EFI and EDK II implementations
- **Automatic Device Detection** - Uses SMBIOS information for device identification
- **Dynamic Device Tree Overlays** - Runtime DTBO loading with parameter support
- **Multiple Boot Standards** - Compatible with systemd-boot (UAPI Boot Loader Specification)
- **Multiple UI Options** - Text, TUI, HII, and GUI menu interfaces
- **Profile-based Configuration** - Flexible configuration system with device profiles
- **Advanced Filtering** - Profile-based loader filtering and conditional loading

## Quick Start

### Install dependencies

Ubuntu / Debian (At least Ubuntu 22.04)

```bash
sudo apt update
sudo apt install -y debhelper gcc g++ git make nasm python3 uuid-dev build-essential
```

Arch Linux

```bash
sudo pacman -Syu
sudo pacman --needed -S gcc git make nasm python
```

### Building

```bash
bash build.sh
```

### Installation

1. Copy the built `embloader.efi` to your ESP partition: 
   - EFI/BOOT/BOOTAA64.EFI (arm64)
   - EFI/BOOT/BOOTX64.EFI (x86_64)
2. Create configuration directory: `mkdir -p /path/to/esp/embloader`
3. Copy configuration files to the embloader directory
4. Set as default boot loader or add to your existing boot menu

## Boot Standards Compatibility

### UAPI Boot Loader Specification

Full compatibility with systemd-boot configuration:
- `/loader/loader.conf` - Main configuration
- `/loader/entries/*.conf` - Boot entries

### Traditional Formats

- **ExtLinux** - Classic syslinux configuration (planned)

### Development Guidelines

- Use consistent C coding style
- Add comprehensive documentation for new features
- Test on multiple platforms when possible
- Follow the existing configuration schema

## License

This project is licensed under the GPL v2.0 License - see the [LICENSE](LICENSE) file for details.

## Support

- **Issues**: Report bugs and feature requests on [GitHub Issues](https://github.com/BigfootACA/embloader/issues)
- **Discussions**: Community discussions on [GitHub Discussions](https://github.com/BigfootACA/embloader/discussions)
- **Documentation**: Comprehensive guides in the [docs](docs/) directory

## Acknowledgments

- [EDK II](https://github.com/tianocore/edk2) - UEFI development framework
- [LVGL](https://lvgl.io/) - Graphics library for GUI interface
- [systemd-boot](https://systemd.io/) - Boot loader specification compatibility
- Device tree community for overlay standards
