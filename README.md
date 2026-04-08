# VXE
Linux kernel HID driver for VXE Dragonfly mice using the COMPX protocol.  
Reports battery capacity, charging state, and voltage through the standard `power_supply` subsystem.  

DPI, polling rate, and other device settings
are accessible via hidraw from userspace.

## Supported Devices
| Device | VID:PID | Connection |
|--------|---------|------------|
| VXE Dragonfly R1 PRO | `3554:f58a` | 1K Dongle (wireless) |
| VXE Dragonfly R1 PRO | `3554:f58c` | Wired (USB-C) |

Want support for another VXE mouse? Please open an issue.

## ATK HUB
If you want to use ATK HUB, see [this guide](https://www.reddit.com/r/linux_gaming/comments/1feizmm/comment/lqhja85/).

## Build
Ensure you use the same compiler that built your kernel.
You can check with:

```sh
cat /proc/version
```

For example, if your kernel was built with clang:

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
