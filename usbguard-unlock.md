# Unlocking the ESP32 serial port from USBGuard

This machine runs `usbguard`, which blocks newly-plugged USB devices by default.
The Clawd Mochi board enumerates as a "USB JTAG/serial debug unit"
(USB ID `303a:1001`) — when usbguard hasn't authorized it, `lsusb` will show the
device but **no `/dev/ttyACM*` node appears**, so the Arduino IDE / serial
monitor can't see the port.

Note: the device name in `usbguard list-devices` is just "USB JTAG/serial
debug unit" — it does **not** say "Espressif" or "Clawd Mochi". Match on the
USB ID `303a:1001` instead, it's more reliable.

## Check if this is the problem

```bash
lsusb | grep 303a:1001         # device shows up here...
ls /dev/ttyACM*                # ...but not here = usbguard is blocking it
```

## One-time unlock (until replugged)

```bash
sudo usbguard allow-device $(sudo usbguard list-devices | grep '303a:1001' | cut -d: -f1)
```

## Permanent allow (survives unplug/replug/reboot)

Adds a persistent rule to `/etc/usbguard/rules.conf` for this exact device:

```bash
sudo usbguard allow-device $(sudo usbguard list-devices | grep '303a:1001' | cut -d: -f1) --permanent
```

After either command, confirm the port appeared:

```bash
ls /dev/ttyACM*
```

## Notes

- `usbguard list-devices` and `usbguard allow-device` both talk to the
  usbguard IPC socket, which is root-only on this machine — there's no
  passwordless sudo rule for it, so this always needs an interactive
  password.
- If you get tired of doing this every time you plug the board in, use the
  `--permanent` form once and usbguard will stop asking for this specific
  device.
