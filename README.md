# VXE
Linux kernel driver for VXE Dragonfly mice. Provides battery level reporting through the standard `power_supply` subsystem.

## Supported Devices
| Device | VID:PID | Status |
|--------|---------|--------|
| VXE Dragonfly R1 PRO (wireless) | `3554:f58a` | Supported |

Want support for another VXE mouse? Please open an issue.

## Features
- [x] Battery level reporting (capacity, charging state)
- [x] Battery voltage reporting
- [ ] DPI / polling rate configuration

## ATK HUB
If you want to use ATK HUB, see [this guide](https://www.reddit.com/r/linux_gaming/comments/1feizmm/comment/lqhja85/).

## Build
Ensure you use the same compiler that built your kernel:

```sh
make CC=clang LD=ld.lld
```

## Usage

```sh
# Load
sudo insmod hid-vxe.ko

# Verify
upower -d | grep -A 15 vxe

# Unload
sudo rmmod hid-vxe
```
