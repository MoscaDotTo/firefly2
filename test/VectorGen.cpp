// Renders firmware effects through the host-test fakes and prints reference
// vectors as JSON to stdout. This is the fidelity ground truth used by the
// JavaScript simulator port (specs/001-web-simulator). See
// specs/001-web-simulator/contracts/reference-vectors.md for the schema.
//
// Usage:
//   mkdir -p build && cd build
//   cmake .. -DBUILD_SIMULATOR=false && make vectorgen
//   ./vectorgen > ../sim/test/vectors/reference.json
//
// This binary is intentionally not part of smalltests/largetests.

#include <FastLED.h>

#include <DeviceDescription.hpp>
#include <Devices.hpp>
#include <Effect.hpp>
#include <Effects.hpp>
#include <FakeLedManager.hpp>
#include <FakeRadio.hpp>
#include <NetworkManager.hpp>
#include <Radio.hpp>
#include <RadioStateMachine.hpp>
#include <Types.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

// The FastLED LCG seed used by FireEffect/RorschachEffect construction (host
// builds skip the ARDUINO-only re-seed from analogRead), and by the
// `primitives.random16First5` sample below.
constexpr uint16_t kFastLedSeed = 1337;

// FireflyEffect::kBlinkPeriod / 2 (private, so duplicated here). See
// lib/effect/FireflyEffect.hpp: kPeriodMs=20000, kSinMultiplier=64,
// kBlinkPeriod=(1<<16)/kSinMultiplier=1024.
constexpr uint16_t kFireflyRandMax = 512;

struct EffectSeeds {
  uint16_t fire;
  uint16_t rorschach;
  uint16_t firefly;
};

// Predicts the constructor-time random offsets consumed by FireEffect,
// FireflyEffect and RorschachEffect (the only effects that consume randomness
// at construction time - verified via `grep -n random lib/effect/*.cpp`), then
// resets both PRNGs so a subsequent LedManager construction reproduces the
// exact same sequence.
//
// Must be called immediately before constructing the LedManager: the
// RadioStateMachine constructor's beginSlave() consumes one libc rand() for
// its timer jitter, which would otherwise shift FireflyEffect's draw.
EffectSeeds PredictEffectSeedsAndResetPrngs() {
  EffectSeeds seeds;

  srand(1);
  random16_set_seed(kFastLedSeed);
  // Construction order (LedManager.cpp): ColorCycle, ContrastBumps, Fire
  // (1st random16 consumer), Firefly (1st libc rand consumer), Lightning,
  // Pride, RainbowBumps, Rainbow, Rorschach (2nd random16 consumer), ...
  seeds.fire = random16();
  seeds.rorschach = random16();

  srand(1);
  seeds.firefly = 0 + (rand() % kFireflyRandMax);

  // Reset again so the real construction below consumes the exact predicted
  // sequence.
  srand(1);
  random16_set_seed(kFastLedSeed);
  return seeds;
}

struct EffectInfo {
  uint8_t index;
  const char *name;
};

// The full computed wire table (LedManager.cpp:12-35): weights
// 2,2,1,2,1,1,4,4,2,4,4 for the random effects (indices 0-26), then 8
// non-random effects (indices 27-34, verified 33=DisplayColorPalette and
// 34=Dark).
const std::vector<EffectInfo> &Effects() {
  static const std::vector<EffectInfo> effects = {
      {0, "Color Cycle"},
      {1, "Color Cycle"},
      {2, "Contrast Bumps"},
      {3, "Contrast Bumps"},
      {4, "Fire"},
      {5, "Firefly"},
      {6, "Firefly"},
      {7, "Lightning"},
      {8, "Pride"},
      {9, "Rainbow Bumps"},
      {10, "Rainbow Bumps"},
      {11, "Rainbow Bumps"},
      {12, "Rainbow Bumps"},
      {13, "Rainbow"},
      {14, "Rainbow"},
      {15, "Rainbow"},
      {16, "Rainbow"},
      {17, "Rorschach"},
      {18, "Rorschach"},
      {19, "Spark"},
      {20, "Spark"},
      {21, "Spark"},
      {22, "Spark"},
      {23, "Swinging Lights"},
      {24, "Swinging Lights"},
      {25, "Swinging Lights"},
      {26, "Swinging Lights"},
      {27, "Swinging Lights (Police)"},
      {28, "Stop Light"},
      {29, "Simple Blink 60ms"},
      {30, "Simple Blink 30ms"},
      {31, "Simple Blink 12ms"},
      {32, "Simple Blink 300ms"},
      {33, "Display Color Palette"},
      {34, "Dark"},
  };
  return effects;
}

// One representative wire index per unique constructed effect object.
const std::vector<uint8_t> &RepresentativeEffectIndices() {
  static const std::vector<uint8_t> indices = {
      0, 2, 4, 5, 7, 8, 9, 13, 17, 19, 23, 27, 28, 29, 30, 31, 32, 33, 34};
  return indices;
}

struct PaletteInfo {
  uint8_t index;
  const char *name;
};

const std::vector<PaletteInfo> &Palettes() {
  static const std::vector<PaletteInfo> palettes = {
      {0, "Red"},
      {1, "Orange"},
      {2, "Yellow"},
      {3, "Green"},
      {4, "Aqua"},
      {5, "Blue"},
      {6, "Purple"},
      {7, "Pink"},
      {8, "Rainbow"},
      {9, "Warm"},
      {10, "Cool"},
      {11, "Yellow-Green"},
      {12, "80s Miami"},
      {13, "Vaporwave"},
      {14, "Cool Popo"},
      {15, "Candy Cane"},
      {16, "Winter Mint"},
      {17, "Fire"},
      {18, "Pastel Rainbow"},
      {19, "Jazz Cup"},
      {20, "Yellow & Double Purp"},
      {21, "Double Rainbow"},
  };
  return palettes;
}

const std::vector<uint8_t> &PaletteGrid() {
  static const std::vector<uint8_t> palettes = {0, 9, 8, 21};
  return palettes;
}

const std::vector<uint32_t> &TimeGrid() {
  static const std::vector<uint32_t> times = {0,     1,           1000,
                                              60000, 2147483648u, 4294967295u};
  return times;
}

struct DeviceInfo {
  const char *name;
  const DeviceDescription &description;
};

const std::vector<DeviceInfo> &DeviceGrid() {
  static const std::vector<DeviceInfo> devices = {
      {"scarf", Devices::scarf},
      {"puck", Devices::puck},
      {"ufo", Devices::ufo},
  };
  return devices;
}

std::string GitDescribe() {
  FILE *pipe = popen("git describe --always --dirty 2>/dev/null", "r");
  if (pipe == nullptr) {
    return "unknown";
  }
  std::string result;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  int status = pclose(pipe);
  if (status != 0 || result.empty()) {
    return "unknown";
  }
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
    result.pop_back();
  }
  return result.empty() ? "unknown" : result;
}

// Minimal, deterministic hand-rolled JSON emitter: integers only, fixed key
// order, no third-party dependencies.

void WriteJsonString(std::ostream &out, const std::string &value) {
  out << '"';
  for (char c : value) {
    if (c == '"' || c == '\\') {
      out << '\\';
    }
    out << c;
  }
  out << '"';
}

void WriteRgb(std::ostream &out, const CRGB &rgb) {
  out << "[" << static_cast<int>(rgb.r) << "," << static_cast<int>(rgb.g) << ","
      << static_cast<int>(rgb.b) << "]";
}

// Owns the fakes needed to render a device's effects, resetting the PRNGs
// before construction so every device's LedManager gets the identical
// predicted effect-construction sequence.
// Resets the PRNGs between the RadioStateMachine member (whose constructor
// consumes a libc rand() for timer jitter) and the LedManager member (whose
// effect construction must consume the predicted sequence).
const DeviceDescription &ResetPrngsBeforeLedManager(
    const DeviceDescription &device) {
  PredictEffectSeedsAndResetPrngs();
  return device;
}

struct DeviceRig {
  explicit DeviceRig(const DeviceDescription &device)
      : radio(),
        network_manager(&radio),
        state_machine(&network_manager),
        manager(ResetPrngsBeforeLedManager(device), &state_machine) {}

  FakeRadio radio;
  NetworkManager network_manager;
  RadioStateMachine state_machine;
  FakeLedManager manager;
};

std::unique_ptr<DeviceRig> BuildRig(const DeviceDescription &device) {
  return std::unique_ptr<DeviceRig>(new DeviceRig(device));
}

void WriteEffectCase(std::ostream &out, DeviceRig *rig, const char *device_name,
                     uint8_t effect_index, uint8_t palette_index,
                     uint32_t time_ms, uint8_t led_count, bool *first) {
  RadioPacket packet;
  packet.writeSetEffect(effect_index, 0, palette_index);
  rig->state_machine.SetEffect(&packet);
  setMillis(time_ms);
  rig->manager.RunEffect();

  if (!*first) {
    out << ",";
  }
  *first = false;
  out << "\n    {\"effectIndex\":" << static_cast<int>(effect_index)
      << ",\"paletteIndex\":" << static_cast<int>(palette_index)
      << ",\"device\":";
  WriteJsonString(out, device_name);
  out << ",\"timeMs\":" << time_ms << ",\"leds\":[";
  for (uint8_t i = 0; i < led_count; i++) {
    if (i > 0) {
      out << ",";
    }
    WriteRgb(out, rig->manager.GetLed(i));
  }
  out << "]}";
}

void WriteControlCase(std::ostream &out, DeviceRig *rig,
                      const char *device_name, const CRGB &rgb,
                      uint32_t time_ms, uint8_t led_count, bool *first) {
  RadioPacket packet;
  packet.writeControl(0, rgb);
  // RadioStateMachine::SetEffect() unconditionally parses the packet as a
  // SET_EFFECT packet (it's meant for effect changes only), so inject the
  // SET_CONTROL packet directly - this mirrors what handleSlaveEvent/
  // handleMasterEvent do for a received SET_CONTROL packet.
  *rig->state_machine.GetSetEffect() = packet;
  setMillis(time_ms);
  rig->manager.RunEffect();

  if (!*first) {
    out << ",";
  }
  *first = false;
  out << "\n    {\"control\":{\"rgb\":";
  WriteRgb(out, rgb);
  out << "},\"device\":";
  WriteJsonString(out, device_name);
  out << ",\"timeMs\":" << time_ms << ",\"leds\":[";
  for (uint8_t i = 0; i < led_count; i++) {
    if (i > 0) {
      out << ",";
    }
    WriteRgb(out, rig->manager.GetLed(i));
  }
  out << "]}";
}

void WritePrimitives(std::ostream &out) {
  out << "\"primitives\":{\n";

  out << "    \"sin16\":[";
  const std::vector<uint16_t> sin16_inputs = {0,     8192,  16384,
                                              32768, 49152, 65535};
  for (size_t i = 0; i < sin16_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"in\":" << sin16_inputs[i]
        << ",\"out\":" << static_cast<int>(sin16(sin16_inputs[i])) << "}";
  }
  out << "],\n";

  out << "    \"sin8\":[";
  const std::vector<uint8_t> sin8_inputs = {0, 64, 128, 192, 255};
  for (size_t i = 0; i < sin8_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"in\":" << static_cast<int>(sin8_inputs[i])
        << ",\"out\":" << static_cast<int>(sin8(sin8_inputs[i])) << "}";
  }
  out << "],\n";

  out << "    \"cubicwave8\":[";
  const std::vector<uint8_t> cubicwave8_inputs = {0, 32, 64, 128, 196};
  for (size_t i = 0; i < cubicwave8_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"in\":" << static_cast<int>(cubicwave8_inputs[i])
        << ",\"out\":" << static_cast<int>(cubicwave8(cubicwave8_inputs[i]))
        << "}";
  }
  out << "],\n";

  out << "    \"ease8InOutApprox\":[";
  const std::vector<uint8_t> ease8_inputs = {0, 63, 64, 128, 191, 192, 255};
  for (size_t i = 0; i < ease8_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"in\":" << static_cast<int>(ease8_inputs[i])
        << ",\"out\":" << static_cast<int>(ease8InOutApprox(ease8_inputs[i]))
        << "}";
  }
  out << "],\n";

  out << "    \"scale8\":[";
  const std::vector<std::pair<uint8_t, uint8_t>> scale8_inputs = {
      {128, 128}, {255, 255}, {1, 255}, {255, 1}};
  for (size_t i = 0; i < scale8_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"i\":" << static_cast<int>(scale8_inputs[i].first)
        << ",\"scale\":" << static_cast<int>(scale8_inputs[i].second)
        << ",\"out\":"
        << static_cast<int>(
               scale8(scale8_inputs[i].first, scale8_inputs[i].second))
        << "}";
  }
  out << "],\n";

  out << "    \"hsv2rgbRainbow\":[";
  const std::vector<CHSV> hsv_inputs = {
      CHSV(0, 255, 255),   CHSV(32, 255, 255),  CHSV(96, 255, 255),
      CHSV(160, 255, 255), CHSV(224, 255, 255), CHSV(0, 0, 200),
      CHSV(33, 241, 249),  CHSV(128, 128, 128),
  };
  for (size_t i = 0; i < hsv_inputs.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    const CHSV &hsv = hsv_inputs[i];
    CRGB rgb = hsv;
    out << "{\"h\":" << static_cast<int>(hsv.h)
        << ",\"s\":" << static_cast<int>(hsv.s)
        << ",\"v\":" << static_cast<int>(hsv.v) << ",\"out\":";
    WriteRgb(out, rgb);
    out << "}";
  }
  out << "],\n";

  // RESEED before sampling the raw LCG stream, done last so it doesn't
  // disturb construction/render seeds above.
  random16_set_seed(kFastLedSeed);
  out << "    \"random16First5\":[";
  for (int i = 0; i < 5; i++) {
    if (i > 0) {
      out << ",";
    }
    out << random16();
  }
  out << "]\n";

  out << "  }";
}

}  // namespace

int main() {
  EffectSeeds seeds = PredictEffectSeedsAndResetPrngs();

  std::ostringstream out;
  out << "{\n";

  out << "  \"meta\":{\n";
  out << "    \"generator\":";
  WriteJsonString(out, "test/VectorGen.cpp");
  out << ",\n    \"firmwareGitDescribe\":";
  WriteJsonString(out, GitDescribe());
  out << ",\n    \"effectSeeds\":{\"Fire\":" << seeds.fire
      << ",\"Firefly\":" << seeds.firefly
      << ",\"Rorschach\":" << seeds.rorschach << "}\n";
  out << "  },\n";

  out << "  \"effects\":[";
  const std::vector<EffectInfo> &effects = Effects();
  for (size_t i = 0; i < effects.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"index\":" << static_cast<int>(effects[i].index) << ",\"name\":";
    WriteJsonString(out, effects[i].name);
    out << "}";
  }
  out << "],\n";

  out << "  \"palettes\":[";
  const std::vector<PaletteInfo> &palettes = Palettes();
  for (size_t i = 0; i < palettes.size(); i++) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"index\":" << static_cast<int>(palettes[i].index) << ",\"name\":";
    WriteJsonString(out, palettes[i].name);
    out << "}";
  }
  out << "],\n";

  out << "  \"cases\":[";
  bool first_case = true;
  for (const DeviceInfo &device_info : DeviceGrid()) {
    std::unique_ptr<DeviceRig> rig = BuildRig(device_info.description);
    uint8_t led_count = device_info.description.GetLedCount();

    for (uint8_t effect_index : RepresentativeEffectIndices()) {
      for (uint8_t palette_index : PaletteGrid()) {
        for (uint32_t time_ms : TimeGrid()) {
          WriteEffectCase(out, rig.get(), device_info.name, effect_index,
                          palette_index, time_ms, led_count, &first_case);
        }
      }
    }
  }

  const std::vector<CRGB> control_rgbs = {CRGB(255, 0, 0), CRGB(12, 34, 56)};
  const std::vector<uint32_t> control_times = {0, 1000};
  for (const DeviceInfo &device_info : DeviceGrid()) {
    std::unique_ptr<DeviceRig> rig = BuildRig(device_info.description);
    uint8_t led_count = device_info.description.GetLedCount();

    for (const CRGB &rgb : control_rgbs) {
      for (uint32_t time_ms : control_times) {
        WriteControlCase(out, rig.get(), device_info.name, rgb, time_ms,
                         led_count, &first_case);
      }
    }
  }
  out << "\n  ],\n";

  out << "  ";
  WritePrimitives(out);
  out << "\n}\n";

  std::cout << out.str();
  return 0;
}
