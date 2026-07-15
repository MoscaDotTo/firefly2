#include "MorseEffect.hpp"

MorseEffect::MorseEffect(const char *message, Mode mode, uint16_t unit_ms)
    : Effect(), mode_(mode), unit_ms_(unit_ms) {
  bool wrote_any = false;
  bool word_has_char = false;
  bool pending_word_gap = false;

  for (const char *p = message; *p != '\0'; p++) {
    if (*p == ' ') {
      if (word_has_char) {
        pending_word_gap = true;
        word_has_char = false;
      }
      continue;
    }

    const char *code = CodeFor(*p);
    if (code == nullptr) {
      continue;
    }

    if (pending_word_gap) {
      Append(false, 7);
      pending_word_gap = false;
    } else if (word_has_char) {
      Append(false, 3);
    }

    for (const char *symbol = code; *symbol != '\0'; symbol++) {
      if (symbol != code) {
        Append(false, 1);
      }
      Append(true, *symbol == '-' ? 3 : 1);
    }
    word_has_char = true;
    wrote_any = true;
  }

  // Trailing word gap, so the looping pattern reads cleanly.
  if (wrote_any) {
    Append(false, 7);
  }
}

CRGB MorseEffect::GetRGB(uint8_t led_index, uint32_t time_ms,
                         const StripDescription &strip,
                         RadioPacket *setEffectPacket) const {
  if (units_.empty()) {
    return {0, 0, 0};
  }

  const uint32_t num_units = units_.size();
  uint32_t index;
  if (mode_ == Mode::kMarquee) {
    // The pattern is printed along the strip and scrolls by one LED per
    // unit_ms. led_index is already Reversed-adjusted by LedManager.
    const uint32_t scroll = time_ms / unit_ms_;
    index = (led_index + scroll) % num_units;
  } else {
    index = (time_ms / unit_ms_) % num_units;
  }
  if (!units_[index]) {
    return {0, 0, 0};
  }

  const ColorPalette &palette =
      palettes()[setEffectPacket->readPaletteIndexFromSetEffect()];
  // Walk through the palette as the message progresses.
  CHSV color = palette.GetGradient((index * MAX_UINT16) / num_units);
  if (!strip.FlagEnabled(Bright)) {
    color.v /= 2;
  }
  return color;
}

void MorseEffect::Append(bool on, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    units_.push_back(on ? 1 : 0);
  }
}

const char *MorseEffect::CodeFor(char c) {
  if (c >= 'a' && c <= 'z') {
    c -= 'a' - 'A';
  }
  switch (c) {
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";
    case '0': return "-----";
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case '6': return "-....";
    case '7': return "--...";
    case '8': return "---..";
    case '9': return "----.";
    default: return nullptr;
  }
}
