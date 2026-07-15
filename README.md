# Firefly2

## Programming

The `fancy-node` device can be programmed either using an STLink, or via USB port. To program via USB, you must first install [stm32loader] using pip (`pip install stm32loader`). You may also need to pass the port (typically `/dev/ttyUSB0`), e.g. `pio run -e fancy-node-usb -t upload --upload-port /dev/ttyUSB0`.

### Saving device description in flash

Both `node` and `fancy-node` support reading the [`DeviceDescription`](lib/device/DeviceDescription.hpp) from flash. The modes are defined by the `DeviceMode` enum within [`DeviceDescription.hpp`](lib/device/DeviceDescription.hpp). They work as follows:

- `CURRENT_FROM_HEADER`: use the `current` device defined in [`Devices.hpp`](lib/device/Devices.hpp)
- `READ_FROM_FLASH`: read the device saved in flash, if present. If not present (determined by the magic, version, and CRC32 of the stored `FlashConfigV1` blob), fall back to `current`.
- `WRITE_TO_FLASH`: write `current` to flash, and then use it. This will only write the device to flash if it is different, to avoid causing flash wear.

The flash format (`FlashConfigV1`, documented in [`DeviceDescription.hpp`](lib/device/DeviceDescription.hpp)) is a 32-byte serialized blob rather than the raw struct, so external tools can write it too: the web configurator can generate a UF2 file containing just the config block (at `0x1E000` on the node), letting you reconfigure a device without recompiling.
