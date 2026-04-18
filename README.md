# VXE (archived)

> **This driver is obsolete.** Linux 7.0 ships an in-tree driver,
> [`hid-kysona`](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/hid/hid-kysona.c),
> that already supports VXE Dragonfly R1 PRO (`3554:f58a` dongle and `3554:f58c` wired).
> Use it instead — no out-of-tree module is required.
>
> Background: Kysona M600 and VXE Dragonfly share the same OEM firmware and HID
> protocol, so upstream added the VXE IDs to the existing Kysona driver.

## Migrating

1. Make sure you're on Linux 7.0 or newer.
2. Unload and uninstall this module if you had it installed:
   ```sh
   sudo rmmod hid-vxe
   ```
3. Plug the mouse in — `hid-kysona` loads automatically and exposes the battery
   via `power_supply`:
   ```sh
   upower -d | grep -A 15 kysona
   ```

## Original description (for historical reference)

Linux kernel HID driver for VXE Dragonfly mice using the COMPX protocol.
Reports battery capacity, charging state, and voltage through the standard
`power_supply` subsystem.

### Supported Devices
| Device | VID:PID | Connection |
|--------|---------|------------|
| VXE Dragonfly R1 PRO | `3554:f58a` | 1K Dongle (wireless) |
| VXE Dragonfly R1 PRO | `3554:f58c` | Wired (USB-C) |

### ATK HUB
If you want to use ATK HUB, see [this guide](https://www.reddit.com/r/linux_gaming/comments/1feizmm/comment/lqhja85/).
