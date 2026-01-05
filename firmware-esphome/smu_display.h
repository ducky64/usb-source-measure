uint32_t intpow10(uint8_t n) {
  static uint32_t lut[] = {
    1, 10, 100, 1000, 10000, 100000,
    1000000, 10000000, 100000000, 1000000000  //, 10000000000, 100000000000
  };
  return lut[n];
}

// Draws some text, optionally inverted.
void drawInverted(display::Display& it, int x, int y, font::Font* font, const char* text, bool invert = true) {
  if (invert) {
    int width, baseline, dummy;
    font->measure(text, &width, &dummy, &baseline, &dummy);
    it.filled_rectangle(x - 1, y, width + 1, baseline);
    it.print(x, y, font, COLOR_OFF, text);
  } else {
    it.print(x, y, font, text);
  }
}

// Return the static width used by drawValue
uint8_t drawValueWidth(font::Font* font, uint8_t numDigits, uint8_t numDigitsDecimal) {
  int width, baseline, dummy;
  font->measure("8", &width, &dummy, &baseline, &dummy);
  // negative, integer digits, integer digits group separator, decimal, decimal digits, decimal group separator
  return width + width * numDigits + (numDigits - 1) / 3 * 2 + (width - 2) + 
      width * numDigitsDecimal + (numDigitsDecimal - 1) / 3 * 2;
}

// Utility for drawing 
// underlineLoc is specified as 0 for the ones digit, 1 for the tens, and -1 for the 1/10s digit, and so on.
void drawValue(display::Display& it, int x, int y, font::Font* font, 
    uint8_t numDigits, uint8_t numDigitsDecimal, float value, int8_t underlineLoc = 127) {
  int width, baseline, dummy;
  font->measure("8", &width, &dummy, &baseline, &dummy);

  int32_t valueDecimal = value * intpow10(numDigitsDecimal + 1);
  if (valueDecimal > 0) {  // do rounding
    valueDecimal = (valueDecimal + 5) / 10;
  } else {
    valueDecimal = (valueDecimal - 5) / 10;
  }
  
  char digits[12] = {0};
  itoa(abs(valueDecimal), digits, 10);
  uint8_t digitsLen = strlen(digits);
  int8_t digitsOffset = (numDigits + numDigitsDecimal) - strlen(digits);  // negative means overflow, positive is blank / zero digits

  char forcedChar = 0;  // if nonzero, all digits replaced with this
  if (isnan(value)) {
    forcedChar = '-';
  } else if (digitsOffset < 0) {  // if number exceeds length
    forcedChar = '+';
  }

  // currentDigit indicates the position being drawn
  // numDigits is one past the maximum integer digit, for the negative sign
  // the most significant integer digit is at numDigits - 1,
  // 0 is the ones digit, and -1 is the first decimal digit, and so on
  for (int8_t currentDigit = numDigits; currentDigit >= -numDigitsDecimal; currentDigit--) {
    int8_t digitsPos = (int8_t)digitsLen - (currentDigit + numDigitsDecimal) - 1;  // position within digits[]
    char thisChar[2] = " ";
    if (forcedChar != 0) {
      thisChar[0] = forcedChar;
    } else {
      if (digitsPos < 0) {
        if (value < 0 && currentDigit == numDigits) {
          thisChar[0] = '-';
        } else if (currentDigit <= 0) {  // zero-pad the ones and decimal positions
          thisChar[0] = '0';
        } else {
          thisChar[0] = ' ';
        }
      } else {
        thisChar[0] = digits[digitsPos];
      }
    }

    if (underlineLoc == currentDigit) {
      drawInverted(it, x, y, font, thisChar);
    } else {
      it.print(x, y, font, thisChar);
    }
    x += width;    

    if (currentDigit > -numDigitsDecimal) {  // insert trailing markers like decimals and 3-digit-group separators
      if (currentDigit == 0) {
        it.print(x - 1, y, font, ".");
        x += width - 2;
      } else if (currentDigit % 3 == 0 && currentDigit != numDigits) {  // do not insert after negative sign
        x += 2;
      }
    }
  }
}

// Given a value, calculate a scaled value (1-999.99) with a SI prefix, if needed.
void siPrefixValue(float value, float *scaledOut, const char** prefixOut) {
  const char* kPrefixes[] = {"", "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};

  if (value == 0) {
    *scaledOut = value;
    *prefixOut = kPrefixes[0];
  } else {
    int8_t prefixIndex = int(log10(abs(value)) / 3);
    prefixIndex = std::max(std::min(prefixIndex, 
        (int8_t)(sizeof(kPrefixes) / sizeof(kPrefixes[0]) - 1)),
        (int8_t)0);
    *scaledOut = value / pow(10.0, prefixIndex * 3);
    *prefixOut = kPrefixes[prefixIndex];
  }
}
