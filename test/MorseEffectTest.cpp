#include <MorseEffect.hpp>

#include "gtest/gtest.h"

namespace {

constexpr uint16_t kUnit = 100;

// Whether the effect is lit at the given time and LED, on a plain strip.
bool IsOnAt(const MorseEffect &effect, uint8_t led_index, uint32_t time_ms) {
  static const StripDescription strip(10, {});
  RadioPacket packet;
  packet.writeSetEffect(0, 0, 0);
  CRGB rgb = effect.GetRGB(led_index, time_ms, strip, &packet);
  return rgb.r != 0 || rgb.g != 0 || rgb.b != 0;
}

bool IsOn(const MorseEffect &effect, uint32_t time_ms) {
  return IsOnAt(effect, 0, time_ms);
}

// Samples the middle of each unit over one loop of the pattern.
std::vector<bool> Pattern(const MorseEffect &effect, uint32_t num_units) {
  std::vector<bool> result;
  for (uint32_t i = 0; i < num_units; i++) {
    result.push_back(IsOn(effect, i * kUnit + kUnit / 2));
  }
  return result;
}

}  // namespace

TEST(MorseEffect, singleDitHasItuTiming) {
  // "E" = "." -> 1 unit on, then the 7-unit inter-loop word gap.
  const MorseEffect effect("E", MorseEffect::Mode::kBlink, kUnit);
  const std::vector<bool> expected = {true, false, false, false,
                                      false, false, false, false};
  EXPECT_EQ(Pattern(effect, 8), expected);

  // The pattern loops.
  EXPECT_TRUE(IsOn(effect, 8 * kUnit + kUnit / 2));
  EXPECT_EQ(IsOn(effect, 3 * kUnit), IsOn(effect, 11 * kUnit));
}

TEST(MorseEffect, ditDahLetterHasSymbolGaps) {
  // "A" = ".-" -> on 1, gap 1, on 3, then word gap 7 = 12 units.
  const MorseEffect effect("A", MorseEffect::Mode::kBlink, kUnit);
  const std::vector<bool> expected = {true,  false, true,  true,
                                      true,  false, false, false,
                                      false, false, false, false};
  EXPECT_EQ(Pattern(effect, 12), expected);
}

TEST(MorseEffect, lettersSeparatedByThreeUnits) {
  // "EE" -> dit, 3-unit letter gap, dit, 7-unit word gap = 12 units.
  const MorseEffect effect("EE", MorseEffect::Mode::kBlink, kUnit);
  const std::vector<bool> expected = {true,  false, false, false,
                                      true,  false, false, false,
                                      false, false, false, false};
  EXPECT_EQ(Pattern(effect, 12), expected);
}

TEST(MorseEffect, wordsSeparatedBySevenUnits) {
  // "E E" -> dit, 7-unit word gap, dit, 7-unit word gap = 16 units.
  const MorseEffect effect("E E", MorseEffect::Mode::kBlink, kUnit);
  const std::vector<bool> expected = {
      true, false, false, false, false, false, false, false,
      true, false, false, false, false, false, false, false};
  EXPECT_EQ(Pattern(effect, 16), expected);

  // Repeated and leading/trailing spaces collapse into single word gaps.
  const MorseEffect spaced("  E   E  ", MorseEffect::Mode::kBlink, kUnit);
  EXPECT_EQ(Pattern(spaced, 16), expected);
}

TEST(MorseEffect, lowercaseMatchesUppercase) {
  const MorseEffect lower("sos", MorseEffect::Mode::kBlink, kUnit);
  const MorseEffect upper("SOS", MorseEffect::Mode::kBlink, kUnit);
  // S(5) + gap(3) + O(11) + gap(3) + S(5) + word gap(7) = 34 units.
  EXPECT_EQ(Pattern(lower, 34), Pattern(upper, 34));
  EXPECT_TRUE(IsOn(lower, kUnit / 2));
}

TEST(MorseEffect, unsupportedCharactersAreSkipped) {
  // Unknown characters between letters must not add gaps.
  const MorseEffect with_junk("E#~E", MorseEffect::Mode::kBlink, kUnit);
  const MorseEffect without("EE", MorseEffect::Mode::kBlink, kUnit);
  EXPECT_EQ(Pattern(with_junk, 12), Pattern(without, 12));

  // A message with nothing encodable stays dark.
  const MorseEffect junk_only("#~!", MorseEffect::Mode::kBlink, kUnit);
  for (uint32_t t = 0; t < 20 * kUnit; t += kUnit / 2) {
    EXPECT_FALSE(IsOn(junk_only, t));
  }
}

TEST(MorseEffect, marqueePrintsPatternAlongStrip) {
  // "A" = ".-" -> [on, off, on, on, on, off x7], 12 units. In marquee mode
  // this is printed spatially: LED i shows unit (i + scroll) % 12.
  const MorseEffect effect("A", MorseEffect::Mode::kMarquee, kUnit);
  const bool expected[12] = {true,  false, true,  true,  true,  false,
                             false, false, false, false, false, false};

  // Before the first scroll step, LED index maps straight onto the pattern.
  for (uint8_t led = 0; led < 12; led++) {
    EXPECT_EQ(IsOnAt(effect, led, kUnit / 2), expected[led]) << "led " << (int)led;
  }
  // Patterns longer than the strip wrap around.
  EXPECT_EQ(IsOnAt(effect, 12, kUnit / 2), expected[0]);
  EXPECT_EQ(IsOnAt(effect, 14, kUnit / 2), expected[2]);
}

TEST(MorseEffect, marqueeScrollsOneLedPerUnit) {
  const MorseEffect effect("SOS", MorseEffect::Mode::kMarquee, kUnit);
  // Advancing time by one unit shifts the whole pattern down one LED:
  // LED i at time t+unit shows what LED i+1 showed at time t.
  for (uint32_t t = 0; t < 34 * kUnit; t += kUnit) {
    for (uint8_t led = 0; led < 20; led++) {
      EXPECT_EQ(IsOnAt(effect, led, t + kUnit + kUnit / 2),
                IsOnAt(effect, led + 1, t + kUnit / 2))
          << "led " << (int)led << " t " << t;
    }
  }
}

TEST(MorseEffect, blinkModeIsUniformAcrossLeds) {
  const MorseEffect effect("SOS", MorseEffect::Mode::kBlink, kUnit);
  for (uint32_t t = 0; t < 34 * kUnit; t += kUnit / 2) {
    EXPECT_EQ(IsOnAt(effect, 0, t), IsOnAt(effect, 19, t)) << "t " << t;
  }
}

TEST(MorseEffect, usesPaletteColor) {
  const MorseEffect effect("T", MorseEffect::Mode::kBlink, kUnit);  // "-" -> on for units 0..2
  const StripDescription strip(10, {});
  RadioPacket packet;
  packet.writeSetEffect(0, 0, 0);  // palette 0 = solid red

  CRGB rgb = effect.GetRGB(0, kUnit / 2, strip, &packet);
  EXPECT_GT(rgb.r, 0);
  EXPECT_EQ(rgb.g, 0);
  EXPECT_EQ(rgb.b, 0);

  // All LEDs show the same thing at the same time.
  CRGB other = effect.GetRGB(7, kUnit / 2, strip, &packet);
  EXPECT_EQ(rgb.r, other.r);
  EXPECT_EQ(rgb.g, other.g);
  EXPECT_EQ(rgb.b, other.b);
}
