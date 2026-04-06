#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#define VXE_REPORT_ID 0x08
#define VXE_COMMAND_LEN 16
#define VXE_BATTERY_TIMEOUT_MS 30000

#define USB_VENDOR_ID_VXE 0x3554
#define USB_DEVICE_ID_VXE_DRAGONFLY_R1_WL_MS 0xf58a

enum vxe_command {
  VXE_CMD_GET_BATTERY_LEVEL = 4,
  VXE_CMD_GET_FIRMWARE_VERSION = 18,
};

struct vxe_device {
  struct hid_device* hdev;
  struct device* dev;

  struct power_supply* battery;
  struct power_supply_desc battery_desc;

  spinlock_t battery_lock;
  struct delayed_work battery_work;

  u8 battery_capacity;
  u32 battery_voltage;
  int battery_status;

  u8 firmware_major;
  u8 firmware_minor;
};

static enum power_supply_property vxe_battery_props[] = {
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_CAPACITY,
  POWER_SUPPLY_PROP_VOLTAGE_NOW,
  POWER_SUPPLY_PROP_SCOPE,
};

static int vxe_checksum(const u8* cmd, int len) {
  u16 sum = VXE_REPORT_ID;
  int i;

  for (i = 0; i < len - 1; i++)
    sum += cmd[i];

  return 0x55 - (sum & 0xff);
}

static int vxe_request_battery(struct hid_device* hdev) {
  u8* buf;
  int ret;

  buf = kzalloc(VXE_COMMAND_LEN + 1, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  buf[0] = VXE_REPORT_ID;
  buf[1] = VXE_CMD_GET_BATTERY_LEVEL;

  buf[VXE_COMMAND_LEN] = vxe_checksum(&buf[1], VXE_COMMAND_LEN);

  ret = hid_hw_raw_request(
    hdev,
    VXE_REPORT_ID,
    buf,
    VXE_COMMAND_LEN + 1,
    HID_OUTPUT_REPORT,
    HID_REQ_SET_REPORT
  );

  kfree(buf);

  if (ret < 0) {
    hid_err(hdev, "battery request failed %d\n", ret);
    return ret;
  }

  return 0;
}

static int vxe_request_firmware_version(struct hid_device* hdev) {
  u8* buf;
  int ret;

  buf = kzalloc(VXE_COMMAND_LEN + 1, GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  buf[0] = VXE_REPORT_ID;
  buf[1] = VXE_CMD_GET_FIRMWARE_VERSION;

  buf[VXE_COMMAND_LEN] = vxe_checksum(&buf[1], VXE_COMMAND_LEN);

  ret = hid_hw_raw_request(
    hdev,
    VXE_REPORT_ID,
    buf,
    VXE_COMMAND_LEN + 1,
    HID_OUTPUT_REPORT,
    HID_REQ_SET_REPORT
  );

  kfree(buf);

  if (ret < 0) {
    hid_err(hdev, "firmware version request failed %d\n", ret);
    return ret;
  }

  return 0;
}

static void vxe_battery_work_handler(struct work_struct* work) {
  struct vxe_device* device = container_of(
    work,
    struct vxe_device,
    battery_work.work
  );

  vxe_request_battery(device->hdev);
}

static int vxe_battery_get_property(
  struct power_supply *psy,
  enum power_supply_property psp,
  union power_supply_propval *val
) {
  struct vxe_device* device = power_supply_get_drvdata(psy);

  switch (psp) {
  case POWER_SUPPLY_PROP_STATUS:
    val->intval = device->battery_status;
    break;
  case POWER_SUPPLY_PROP_CAPACITY:
    val->intval = device->battery_capacity;
    break;
  case POWER_SUPPLY_PROP_VOLTAGE_NOW:
    val->intval = device->battery_voltage;
    break;
  case POWER_SUPPLY_PROP_SCOPE:
    val->intval = POWER_SUPPLY_SCOPE_DEVICE;
    break;
  default:
    return -EINVAL;
  }

  return 0;
}

static int vxe_raw_event(
  struct hid_device *hdev,
  struct hid_report *report,
  u8 *data,
  int size
) {
  unsigned long flags;

  struct vxe_device* device = hid_get_drvdata(hdev);
  if (!device)
    return 0;

  hid_info(hdev, "--- VXE RAW EVENT ---\n");
  hid_info(hdev, "raw_event: size=%d data=%*ph\n", size, size, data);
  hid_info(hdev, "---------------------\n");

  if (size < 8 || data[0] != 0x08)
    return 0;

  switch (data[1]) {
  case VXE_CMD_GET_BATTERY_LEVEL:
    device->battery_capacity = data[6];
      device->battery_status = data[7]
        ? POWER_SUPPLY_STATUS_CHARGING
        : POWER_SUPPLY_STATUS_DISCHARGING;
      device->battery_voltage = ((data[8] << 8) | data[9]) * 1000;

    if (device->battery)
      power_supply_changed(device->battery);

    spin_lock_irqsave(&device->battery_lock, flags);
    schedule_delayed_work(&device->battery_work, msecs_to_jiffies(VXE_BATTERY_TIMEOUT_MS));
    spin_unlock_irqrestore(&device->battery_lock, flags);
    break;
  case VXE_CMD_GET_FIRMWARE_VERSION:
    device->firmware_major = data[6];
    device->firmware_minor = data[7];

    hid_info(
      hdev,
      "firmware version: v%d.%02x\n",
      device->firmware_major,
      device->firmware_minor
    );
    break;
  }

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

  // 0xff050000 is the vendor space. This mouse is already available, so
  // Interfaces outside the vendor space will be treated as hid-generic.
  if (hdev->collection->usage != 0xFF050000) {
    hid_set_drvdata(hdev, NULL);
    ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
      if (ret)
        hid_err(hdev, "hid start failed (reason: %d)\n", ret);
    return ret;
  }

  hid_info(hdev, "collection usage: 0x%08x\n", hdev->collection->usage);

  ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
  if (ret) {
    hid_err(hdev, "hid start failed (reason: %d)\n", ret);
    return ret;
  }

  ret = hid_hw_open(hdev);
  if (ret) {
    hid_err(hdev, "hid open failed (reason: %d)\n", ret);
    hid_hw_stop(hdev);
    return ret;
  }

  name = devm_kasprintf(device->dev, GFP_KERNEL, "vxe-%d-battery", hdev->id);
  if (!name) {
    hid_hw_stop(hdev);
    return -ENOMEM;
  }

  device->battery_desc.name = name;
  device->battery_desc.type = POWER_SUPPLY_TYPE_BATTERY;
  device->battery_desc.properties = vxe_battery_props;
  device->battery_desc.num_properties = ARRAY_SIZE(vxe_battery_props);
  device->battery_desc.get_property = vxe_battery_get_property;

  device->battery = devm_power_supply_register(
    device->dev,
    &device->battery_desc,
    &(struct power_supply_config){ .drv_data = device }
  );

  if (IS_ERR(device->battery)) {
    hid_err(hdev, "battery register failed\n");
    hid_hw_close(hdev);
    hid_hw_stop(hdev);
    return PTR_ERR(device->battery);
  }

  INIT_DELAYED_WORK(&device->battery_work, vxe_battery_work_handler);
  vxe_request_firmware_version(hdev);
  vxe_request_battery(hdev);
  schedule_delayed_work(&device->battery_work, msecs_to_jiffies(VXE_BATTERY_TIMEOUT_MS));

  return 0;
}

static void vxe_remove(struct hid_device *hdev) {
  struct vxe_device *device = hid_get_drvdata(hdev);

  if (device) {
    cancel_delayed_work_sync(&device->battery_work);
    hid_hw_close(hdev);
  }
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
  .raw_event = vxe_raw_event,
};

module_hid_driver(vxe_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HATO <hato.flavius@gmail.com>");
MODULE_DESCRIPTION("HID driver for VXE Dragonfly drivers");
