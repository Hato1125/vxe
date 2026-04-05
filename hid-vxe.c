#include "asm-generic/errno-base.h"
#include "linux/array_size.h"
#include "linux/power_supply.h"
#include <linux/gfp_types.h>
#include <linux/hid.h>
#include <linux/module.h>

#define USB_VENDOR_ID_VXE 0x3554
#define USB_DEVICE_ID_VXE_DRAGONFLY_R1_WL_MS 0xf58a

struct vxe_device {
  struct hid_device* hdev;
  struct device* dev;

  struct power_supply* battery;
  struct power_supply_desc battery_desc;

  u8 battery_capacity;
  int battery_status;
};

static enum power_supply_property vxe_battery_props[] = {
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_CAPACITY,
  POWER_SUPPLY_PROP_SCOPE,
};

static int vxe_battery_get_property(
  struct power_supply *psy,
  enum power_supply_property psp,
  union power_supply_propval *val
) {
  return 0;
}

static int vxe_probe(struct hid_device *hdev, const struct hid_device_id *id) {
  int ret;
  char* name;
  struct vxe_device* device;

  device = devm_kzalloc(&hdev->dev, sizeof(*device), GFP_KERNEL);
  if (!device)
    return -ENOMEM;

  hid_set_drvdata(hdev, device);
  dev_set_drvdata(&hdev->dev, device);

  device->hdev = hdev;
  device->dev = &hdev->dev;

  ret = hid_parse(hdev);
  if (ret) {
    hid_err(hdev, "parse failed (reason: %d)\n", ret);
    return ret;
  }

  // 0xff050000 is the vendor space. Since this mouse is already usable,
  // we will leave the non-vendor space interfaces to the standard driver.
  if (hdev->collection->usage != 0xFF050000)
    return -ENODEV;

  hid_info(hdev, "collection usage: 0x%08x\n", hdev->collection->usage);

  ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
  if (ret) {
    hid_err(hdev, "hid start failed (reason: %d)\n", ret);
    return ret;
  }

  name = devm_kasprintf(device->dev, GFP_KERNEL, "vxe-%d-battery", hdev->id);
  if (!name) {
    hid_hw_stop(hdev);
    return -ENOMEM;
  }

  device->battery = NULL;

  device->battery_desc.name = name;
  device->battery_desc.type = POWER_SUPPLY_TYPE_BATTERY;
  device->battery_desc.properties = vxe_battery_props;
  device->battery_desc.num_properties = ARRAY_SIZE(vxe_battery_props);
  device->battery_desc.get_property = vxe_battery_get_property;

  return 0;
}

static void vxe_remove(struct hid_device *hdev) {
  hid_hw_stop(hdev);
}

static const struct hid_device_id vxe_devices[] = {
  {
    HID_USB_DEVICE(USB_VENDOR_ID_VXE, USB_DEVICE_ID_VXE_DRAGONFLY_R1_WL_MS),
  },
  { }
};

static struct hid_driver vxe_driver = {
  .name = "vxe",
  .id_table = vxe_devices,
  .probe = vxe_probe,
  .remove = vxe_remove,
};

module_hid_driver(vxe_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HATO <hato.flavius@gmail.com>");
MODULE_DESCRIPTION("HID driver for VXE Dragonfly drivers");
