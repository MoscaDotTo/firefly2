#ifndef __MORSE_EFFECT_HPP__
#define __MORSE_EFFECT_HPP__

#include <Types.hpp>
#include <vector>

#include "Effect.hpp"

/**
 * Displays a message in morse code.
 *
 * Standard ITU timing, measured in units of unit_ms: dit = 1 on, dah = 3 on,
 * gap between symbols = 1 off, between letters = 3 off, between words = 7
 * off. The pattern loops with a trailing word gap so repeats read cleanly.
 *
 * In kBlink mode the whole device blinks the message in unison. In kMarquee
 * mode the pattern is laid out spatially (one time unit per LED, dah = 3 lit
 * pixels) and scrolls along the strip by one LED per unit_ms; patterns longer
 * than the strip wrap around.
 *
 * The message is fixed at construction. Supports A-Z, a-z, 0-9, and spaces;
 * other characters are skipped.
 */
class MorseEffect : public Effect {
 public:
  enum class Mode {
    kBlink,
    kMarquee,
  };

  static constexpr uint16_t kDefaultUnitMs = 120;

  explicit MorseEffect(const char *message, Mode mode = Mode::kBlink,
                       uint16_t unit_ms = kDefaultUnitMs);

  /** Gets the value of a specific LED at a specific time. */
  CRGB GetRGB(uint8_t led_index, uint32_t time_ms,
              const StripDescription &strip,
              RadioPacket *setEffectPacket) const override;

 private:
  /** Returns the morse code for c (e.g. ".-"), or nullptr if unsupported. */
  static const char *CodeFor(char c);

  void Append(bool on, uint8_t count);

  const Mode mode_;
  const uint16_t unit_ms_;

  /** One entry per morse time unit: 1 = LED on, 0 = off. */
  std::vector<uint8_t> units_;
};
#endif
