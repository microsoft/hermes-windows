/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "hermes/Support/UTF8.h"

namespace hermes {

void encodeUTF8(char *&dst, uint32_t cp) {
  char *d = dst;
  if (cp <= 0x7F) {
    *d = (char)cp;
    ++d;
  } else if (cp <= 0x7FF) {
    d[1] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[0] = (cp & 0x1F) | 0xC0;
    d += 2;
  } else if (cp <= 0xFFFF) {
    d[2] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[1] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[0] = (cp & 0x0F) | 0xE0;
    d += 3;
  } else if (cp <= 0x1FFFFF) {
    d[3] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[2] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[1] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[0] = (cp & 0x07) | 0xF0;
    d += 4;
  } else if (cp <= 0x3FFFFFF) {
    d[4] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[3] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[2] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[1] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[0] = (cp & 0x03) | 0xF8;
    d += 5;
  } else {
    d[5] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[4] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[3] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[2] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[1] = (cp & 0x3F) | 0x80;
    cp >>= 6;
    d[0] = (cp & 0x01) | 0xFC;
    d += 6;
  }
  dst = d;
}

char32_t readUTF16WithReplacements(
    const char16_t *&cur,
    const char16_t *end) noexcept {
  char16_t c = *cur++;
  // ASCII fast-path.
  if (LLVM_LIKELY(c <= 0x7F)) {
    return c;
  }

  if (isLowSurrogate(c)) {
    // Unpaired low surrogate.
    return UNICODE_REPLACEMENT_CHARACTER;
  } else if (isHighSurrogate(c)) {
    // Leading high surrogate. See if the next character is a low surrogate.
    if (cur == end || !isLowSurrogate(*cur)) {
      // Trailing or unpaired high surrogate.
      return UNICODE_REPLACEMENT_CHARACTER;
    } else {
      // Decode surrogate pair and increment, because we consumed two chars.
      return decodeSurrogatePair(c, *cur++);
    }
  } else {
    // Not a surrogate.
    return c;
  }
}

size_t utf8LengthWithReplacements(llvh::ArrayRef<char16_t> input) {
  size_t length{0};
  for (const char16_t *cur = input.begin(), *end = input.end(); cur < end;) {
    char32_t c32 = readUTF16WithReplacements(cur, end);
    if (LLVM_LIKELY(c32 <= 0x7F)) {
      ++length;
    } else if (c32 <= 0x7FF) {
      length += 2;
    } else if (c32 <= 0xFFFF) {
      length += 3;
    } else if (c32 <= 0x1FFFFF) {
      length += 4;
    } else if (c32 <= 0x3FFFFFF) {
      length += 5;
    } else {
      length += 6;
    }
  }

  return length;
}

size_t convertUTF16ToUTF8WithReplacements(
    llvh::ArrayRef<char16_t> input,
    char *buf,
    size_t bufSize) {
  char *curBuf = buf;
  char *endBuf = buf + bufSize;
  for (const char16_t *cur = input.begin(), *end = input.end();
       cur < end && curBuf < endBuf;) {
    char32_t c32 = readUTF16WithReplacements(cur, end);

    char buff[UTF8CodepointMaxBytes];
    char *ptr = buff;
    encodeUTF8(ptr, c32);
    size_t u8Length = static_cast<size_t>(ptr - buff);
    if (curBuf + u8Length <= endBuf) {
      std::char_traits<char>::copy(curBuf, buff, u8Length);
      curBuf += u8Length;
    } else {
      break;
    }
  }

  return static_cast<size_t>(curBuf - buf);
}

bool convertUTF16ToUTF8WithReplacements(
    std::string &out,
    llvh::ArrayRef<char16_t> input,
    size_t maxCharacters) {
  out.clear();
  out.reserve(input.size());
  // Stop early if we've reached currNumCharacters worth of UTF-8 characters.
  size_t currNumCharacters = 0;
  if (!maxCharacters) {
    // Condition checks are easier if this number is set to the max value.
    maxCharacters = std::numeric_limits<size_t>::max();
  }
  for (auto cur = input.begin(), end = input.end();
       cur < end && currNumCharacters < maxCharacters;
       ++currNumCharacters) {
    char16_t c = cur[0];
    // ASCII fast-path.
    if (LLVM_LIKELY(c <= 0x7F)) {
      out.push_back(static_cast<char>(c));
      ++cur;
      continue;
    }

    char32_t c32 = readUTF16WithReplacements(cur, end);

    char buff[UTF8CodepointMaxBytes];
    char *ptr = buff;
    encodeUTF8(ptr, c32);
    out.append(buff, ptr);
  }
  return currNumCharacters < maxCharacters;
}

void convertUTF16ToUTF8WithSingleSurrogates(
    std::string &dest,
    llvh::ArrayRef<char16_t> input) {
  dest.clear();
  dest.reserve(input.size());
  for (char16_t c : input) {
    // ASCII fast-path.
    if (LLVM_LIKELY(c <= 0x7F)) {
      dest.push_back(static_cast<char>(c));
      continue;
    }
    char32_t c32 = c;
    char buff[UTF8CodepointMaxBytes];
    char *ptr = buff;
    encodeUTF8(ptr, c32);
    dest.insert(dest.end(), buff, ptr);
  }
}

bool isAllASCII(const uint8_t *start, const uint8_t *end) {
  const uint8_t *cursor = start;
  size_t len = end - start;
  static_assert(
      sizeof(uint32_t) == 4 && alignof(uint32_t) <= 4,
      "uint32_t must be 4 bytes and cannot be more than 4 byte aligned");

  if (len >= 4) {
    // Step by 1s until aligned for uint32_t.
    uint8_t mask = 0;
    while ((uintptr_t)cursor % alignof(uint32_t)) {
      mask |= *cursor++;
      len -= 1;
    }
    if (mask & 0x80u) {
      return false;
    }

    // Now that we are aligned, step by 4s.
    while (len >= 4) {
      uint32_t val = *(const uint32_t *)cursor;
      if (val & 0x80808080u) {
        return false;
      }
      cursor += 4;
      len -= 4;
    }
  }
  assert(len < 4 && "Length should now be less than 4");
  uint8_t mask = 0;
  while (len--) {
    mask |= *cursor++;
  }
  if (mask & 0x80u)
    return false;
  return true;
}

} // namespace hermes
