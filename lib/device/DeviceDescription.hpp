#ifndef __DEVICE_DESCRIPTION_HPP__
#define __DEVICE_DESCRIPTION_HPP__

#include <Types.hpp>
#include <vector>

#include "StripDescription.hpp"

enum class DeviceMode {
  CURRENT_FROM_HEADER,
  READ_FROM_FLASH,
  WRITE_TO_FLASH,
};

class DeviceDescription {
 public:
  /**
   * @brief How many milliamps this device can support at 5v.
   *
   * This is used as a safety mechanism to prevent the LEDs from pulling too
   * much power.
   */
  const uint32_t milliamps_supported;

  const std::vector<StripDescription> strips;

  explicit DeviceDescription(const uint32_t milliamps_supported,
                             const std::vector<StripDescription> strips);

  uint8_t GetLedCount() const;

  // The FlashConfigV1 serialization format, 32 bytes little-endian:
  //
  //   offset  size  field
  //   0       4     magic "FFLY"
  //   4       1     version (1)
  //   5       1     strip_count (1..kMaxStripsV1)
  //   6       2     reserved (0)
  //   8       4     milliamps_supported
  //   12      16    strips[8]: {led_count, flags} each
  //   28      4     CRC32 (IEEE) of bytes 0..27
  //
  // This format is shared with external tools (e.g. the web configurator),
  // which write it to flash directly - keep them in sync.
  static constexpr uint8_t kMaxStripsV1 = 8;
  static constexpr uint8_t kSerializedSizeV1 = 32;

  /**
   * Serializes this device into the FlashConfigV1 format. `out` must have
   * space for kSerializedSizeV1 bytes. Returns false if this device cannot be
   * represented (no strips, or more than kMaxStripsV1).
   */
  bool SerializeV1(uint8_t *out) const;

  /**
   * Parses a FlashConfigV1 blob of kSerializedSizeV1 bytes. Returns nullptr
   * if the blob is invalid (magic, version, strip count, or CRC mismatch).
   * The caller owns the returned object.
   */
  static DeviceDescription *FromV1(const uint8_t *data);
};
#endif  // __DEVICE_DESCRIPTION_HPP__
