# Firefly2

## Web simulator (test without hardware)

```bash
python3 -m http.server 8642 -d sim
# open http://localhost:8642/
```

A zero-dependency browser simulator of the whole system: every effect and palette on the real device catalog, synced to a scrubbable network clock, with master-mode autoplay and control overrides. Byte-exact against the firmware (enforced by tests: `npm test`). See [docs/simulator.md](docs/simulator.md).


## Programming

The `fancy-node` device can be programmed either using an STLink, or via USB port. To program via USB, you must first install [stm32loader] using pip (`pip install stm32loader`). You may also need to pass the port (typically `/dev/ttyUSB0`), e.g. `pio run -e fancy-node-usb -t upload --upload-port /dev/ttyUSB0`.

### Saving device description in flash

Both `node` and `fancy-node` support reading the [`DeviceDescription`](lib/device/DeviceDescription.hpp) from flash. The modes are defined by the `DeviceMode` enum within [`DeviceDescription.hpp`](lib/device/DeviceDescription.hpp). They work as follows:

- `CURRENT_FROM_HEADER`: use the `current` device defined in [`Devices.hpp`](lib/device/Devices.hpp)
- `READ_FROM_FLASH`: read the device saved in flash, if present. If not present (determined by the validity of `check_value`), fall back to `current`.
- `WRITE_TO_FLASH`: write `current` to flash, and then use it. This will only write the device to flash if it is different, to avoid causing flash wear.
