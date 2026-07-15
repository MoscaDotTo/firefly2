#include "DeviceDescription.hpp"

#include <functional>
#include <numeric>
#include <vector>

namespace {

uint32_t Crc32(const uint8_t *data, uint32_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (uint32_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
    }
  }
  return crc ^ 0xFFFFFFFF;
}

std::vector<StripFlag> FlagsFromRaw(uint8_t raw) {
  std::vector<StripFlag> flags;
  for (uint8_t bit = 0; bit < 8; bit++) {
    if (raw & (1 << bit)) {
      flags.push_back(static_cast<StripFlag>(1 << bit));
    }
  }
  return flags;
}

void WriteUint32Le(uint8_t *out, uint32_t value) {
  out[0] = value & 0xFF;
  out[1] = (value >> 8) & 0xFF;
  out[2] = (value >> 16) & 0xFF;
  out[3] = (value >> 24) & 0xFF;
}

uint32_t ReadUint32Le(const uint8_t *in) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
         ((uint32_t)in[3] << 24);
}

}  // namespace

DeviceDescription::DeviceDescription(uint32_t milliamps_supported,
                                     const std::vector<StripDescription> strips)
    : milliamps_supported(milliamps_supported), strips(strips) {}

uint8_t DeviceDescription::GetLedCount() const {
  uint16_t led_count = 0;
  for (const StripDescription& strip : strips) {
    led_count += strip.led_count;
  }
  return led_count;
}

bool DeviceDescription::SerializeV1(uint8_t *out) const {
  if (strips.size() == 0 || strips.size() > kMaxStripsV1) {
    return false;
  }

  memset(out, 0, kSerializedSizeV1);
  out[0] = 'F';
  out[1] = 'F';
  out[2] = 'L';
  out[3] = 'Y';
  out[4] = 1;  // version
  out[5] = strips.size();
  // Bytes 6-7 are reserved, left zero.
  WriteUint32Le(out + 8, milliamps_supported);
  for (size_t i = 0; i < strips.size(); i++) {
    out[12 + i * 2] = strips[i].led_count;
    out[13 + i * 2] = strips[i].RawFlags();
  }
  WriteUint32Le(out + 28, Crc32(out, 28));
  return true;
}

DeviceDescription *DeviceDescription::FromV1(const uint8_t *data) {
  if (data[0] != 'F' || data[1] != 'F' || data[2] != 'L' || data[3] != 'Y') {
    return nullptr;
  }
  if (data[4] != 1) {
    return nullptr;
  }
  const uint8_t strip_count = data[5];
  if (strip_count == 0 || strip_count > kMaxStripsV1) {
    return nullptr;
  }
  if (Crc32(data, 28) != ReadUint32Le(data + 28)) {
    return nullptr;
  }

  std::vector<StripDescription> strips;
  for (uint8_t i = 0; i < strip_count; i++) {
    strips.push_back(StripDescription(data[12 + i * 2],
                                      FlagsFromRaw(data[13 + i * 2])));
  }
  return new DeviceDescription(ReadUint32Le(data + 8), strips);
}
