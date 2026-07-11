# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firefly2 is a wireless LED control system for Burning Man art installations, bikes, and wearables. Devices (SAMD/STM32/ESP32 boards driving WS2812 strips) self-organize over 915 MHz RFM69 radios into a flood mesh with exactly one master, share a network clock via heartbeats, and render effects as pure functions of network time — so every device animates in sync without streaming frames.

**Detailed research notes live in [`docs/`](docs/index.md)** — start with [docs/architecture.md](docs/architecture.md) for the mental model, then the per-subsystem notes (radio/network, LED/effects, devices, build/test, hardware). Consult them before working in an unfamiliar subsystem; update them when you change documented behavior.

## Build System

### PlatformIO (firmware)

```bash
pio run -e node                  # SAMD rfboard node
pio run -e fancy-node            # STM32G030 node
pio run -e controller            # STM32G070 handheld controller
pio run -e dmx                   # ESP32 DMX bridge
pio run -e fancy-node -t upload  # flash via ST-Link
pio run -e fancy-node-usb -t upload --upload-port /dev/ttyUSB0  # serial bootloader (needs ademuri stm32loader fork)
pio device monitor               # 115200 baud
```

Each env compiles one `src/devices/<target>/` folder plus shared `src/arduino` + `src/generic`. `range_test`, `remote`, and `trellis` envs are commented out (trellis uses an obsolete API and no longer compiles). Note: `[env:node]`'s bossac `platform_packages` entry is OS-specific — uncomment the right one for your OS (`node-arm64` is the Apple-silicon variant). See [docs/build-and-test.md](docs/build-and-test.md) for every env's pins, flags, and pinned library forks.

### Web simulator (browser, no hardware)

```bash
python3 -m http.server 8642 -d sim        # then open http://localhost:8642/
node --test "sim/test/cases/*.test.mjs"   # headless suite (or: npm test)
npm ci && npm run lint                    # ESLint over sim/ (dev-only dep)
```

`sim/` is a zero-dependency JS port of the effect engine for testing shows and protocol behavior without hardware — drivable via `window.sim`, byte-exact against firmware via committed reference vectors (`vectorgen` CMake target regenerates them). See [docs/simulator.md](docs/simulator.md). When you change a firmware effect, regenerate vectors and update the matching `sim/js/effects/` port.

### CMake (host tests)

```bash
mkdir -p build && cd build
cmake ..            # -DBUILD_SIMULATOR=false to skip the SDL simulator
make && make test
./smalltests --gtest_filter=RadioStateMachineTest*
./largetests        # InvalidPacketTest fuzz only (slow, hence separate)
```

Host builds compile the platform-independent core (`lib/` + `src/generic/`) against fakes with **ASan + UBSan always on**. `FakeNetwork` (test/) simulates 5 full nodes with configurable packet loss for deterministic mesh testing.

## Linting & CI

```bash
./lint.sh check     # clang-format dry-run (Google style) — CI enforces this
./lint.sh format    # format in place
./lint.sh tidy      # clang-tidy (no .clang-tidy config; runs with defaults)
./ci.sh             # what GitHub Actions runs: cmake (no simulator) + smalltests + largetests
```

CI also builds the `node` and `fancy-node` PlatformIO envs on every push (not `controller` or `dmx`).

## Architecture (short version)

Per-device layering — main loop is `state_machine.Tick(); led_manager->RunEffect();`:

- **Radio**: `Radio` (abstract, `lib/radio/`) → `RadioHeadRadio` (RFM69, 915 MHz, 13 dBm, `src/arduino/`) or `FakeRadio` (tests). Packets: HEARTBEAT (network time), CLAIM_MASTER, SET_EFFECT (effect/delay/palette), SET_CONTROL (delay/RGB). 58-byte max payload.
- **Mesh**: `NetworkManager` (`src/generic/`) — flood rebroadcast with a 5-deep recent-id dedup cache. Packet ids 0 and 1 are reserved (sentinel / test-wins-election).
- **Protocol**: `RadioStateMachine` (`src/generic/`) — all devices boot as Slave; jittered 5–7 s silence promotes to Master; masters heartbeat every 1 s, change effects randomly every 60 s, rebroadcast the current effect every 2 s; dueling masters resolve by id comparison. Slaves derive `millis_offset_` from heartbeats; `GetNetworkMillis()` is the shared animation clock.
- **LEDs**: `LedManager` (`lib/led_manager/`) → `FastLedManager` (hardware) / `FakeLedManager` (tests) / `SimulatorLedManager` (SDL desktop). Effects implement one method: `GetRGB(led_index, time_ms, strip, packet)` — there is no Init/Calculate lifecycle. Weighted random selection is encoded by duplicate pointers in the registry vector. Power limiting is FastLED's `setMaxPowerInVoltsAndMilliamps(5, device.milliamps_supported)`.
- **Device config**: `DeviceDescription`/`StripDescription` (`lib/device/`) — strips with flags (Tiny, Bright, Circular, Mirrored, Reversed, Controller, Dim, Off) and a milliamp budget. `Devices::current` in `Devices.hpp` selects the build's target hardware. `DeviceMode` optionally stores the description in flash (CURRENT_FROM_HEADER / READ_FROM_FLASH / WRITE_TO_FLASH).

## Invariants — do not break these

- `DisplayColorPaletteEffect` and `DarkEffect` must remain the **last two** registered effects (`LedManager.cpp`); external code assumes "dark" is the final index.
- Effect and palette indices are single wire bytes; total registered effects must stay < 256 (asserted in `AddEffect`).
- Invalid/unknown radio packets must never crash — `InvalidPacketTest` fuzzes ids × types × oversized lengths. Keep packet-type `switch` statements tolerant of unknown values.
- `RadioStateMachine::Tick()`'s once-then-twice `RadioTick` pattern works around a real hardware hang — read the comments there before touching it.
- The `DEBUG` macro in `lib/debug/Debug.hpp` must stay commented out; `DebugTest` fails CI otherwise.
- `RunEffect` handles `Reversed`/`Dim`/`Off` centrally; all other strip flags are each effect's responsibility.
- New effects must pass `EffectsTest`'s fuzz (every palette, 0–255 LEDs, multi-strip Tiny/Circular devices).
- On the SAMD node, the watchdog timeout is ~128 ms — long blocking work in the loop will reset the board.
- The web simulator mirrors the firmware: `sim/js/effects/registry.js` must match `LedManager.cpp` registration (order, weights, last-two invariant), and `sim/test/vectors/reference.json` must be regenerated (`vectorgen`) whenever firmware effect rendering changes — the sim test suite fails on drift.

## Spec-Driven Development

Spec-kit is installed (`.specify/` + `.claude/skills/speckit-*`). For substantial features, use the speckit workflow: `/speckit-specify` → `/speckit-plan` → `/speckit-tasks` → `/speckit-implement` (optionally `/speckit-clarify` before planning and `/speckit-analyze` before implementing). Project principles, if established, live in `.specify/memory/constitution.md`.
