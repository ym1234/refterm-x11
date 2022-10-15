// Modified from pango under GPL2
// You can view the license here: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

#include "emoji_table.h"

#define ARRAYLEN(arr) (sizeof(arr)/sizeof(*arr))
static inline bool
Is_Emoji_Keycap_Base (uint32_t ch)
{
  return (ch >= '0' && ch <= '9') || ch == '#' || ch == '*';
}

static inline bool
Is_Regional_Indicator (uint32_t ch)
{
  return (ch >= 0x1F1E6 && ch <= 0x1F1FF);
}

#define kCombiningEnclosingCircleBackslashCharacter 0x20E0
#define kCombiningEnclosingKeycapCharacter 0x20E3
#define kVariationSelector15Character 0xFE0E
#define kVariationSelector16Character 0xFE0F
#define kZeroWidthJoinerCharacter 0x200D

bool bsearch_interval(uint32_t ch, const struct Interval *interval, size_t len) {
  size_t low = 0;
  size_t high = len;
  while (high > low) {
    size_t mid = (low + high) / 2;
    struct Interval midint = interval[mid];
    if (ch < midint.start)
      high = mid;
    else if (ch > midint.end)
      low = mid + 1;
    else
      return true;
  }
  return false;
}

#define DEFINE_Is_(name) \
static inline bool \
Is_##name (uint32_t ch) \
{ \
  return ch >= name##_table[0].start && \
         bsearch_interval (ch, \
                           name##_table, \
                           ARRAYLEN (name##_table)); \
}

DEFINE_Is_(Emoji)
DEFINE_Is_(Emoji_Presentation)
DEFINE_Is_(Emoji_Modifier)
DEFINE_Is_(Emoji_Modifier_Base)
DEFINE_Is_(Extended_Pictographic)

static inline bool
Is_Emoji_Emoji_Default (uint32_t ch)
{
  return Is_Emoji_Presentation (ch);
}

enum EmojiScannerCategory {
  EMOJI = 0,
  EMOJI_TEXT_PRESENTATION = 1,
  EMOJI_EMOJI_PRESENTATION = 2,
  EMOJI_MODIFIER_BASE = 3,
  EMOJI_MODIFIER = 4,
  EMOJI_VS_BASE = 5,
  REGIONAL_INDICATOR = 6,
  KEYCAP_BASE = 7,
  COMBINING_ENCLOSING_KEYCAP = 8,
  COMBINING_ENCLOSING_CIRCLE_BACKSLASH = 9,
  ZWJ = 10,
  VS15 = 11,
  VS16 = 12,
  TAG_BASE = 13,
  TAG_SEQUENCE = 14,
  TAG_TERM = 15,
  MaxEmojiScannerCategory = 16
};

static inline unsigned char
EmojiSegmentationCategory (uint32_t codepoint)
{
  /* Specific ones first. */
  if (('a' <= codepoint && codepoint <= 'z') ||
      ('A' <= codepoint && codepoint <= 'Z') ||
      codepoint == ' ')
    return MaxEmojiScannerCategory;

  if ('0' <= codepoint && codepoint <= '9')
    return KEYCAP_BASE;

  switch (codepoint)
    {
    case kCombiningEnclosingKeycapCharacter:
      return COMBINING_ENCLOSING_KEYCAP;
    case kCombiningEnclosingCircleBackslashCharacter:
      return COMBINING_ENCLOSING_CIRCLE_BACKSLASH;
    case kZeroWidthJoinerCharacter:
      return ZWJ;
    case kVariationSelector15Character:
      return VS15;
    case kVariationSelector16Character:
      return VS16;
    case 0x1F3F4:
      return TAG_BASE;
    case 0xE007F:
      return TAG_TERM;
    default: ;
    }

  if ((0xE0030 <= codepoint && codepoint <= 0xE0039) ||
      (0xE0061 <= codepoint && codepoint <= 0xE007A))
    return TAG_SEQUENCE;

  if (Is_Emoji_Modifier_Base (codepoint))
    return EMOJI_MODIFIER_BASE;
  if (Is_Emoji_Modifier (codepoint))
    return EMOJI_MODIFIER;
  if (Is_Regional_Indicator (codepoint))
    return REGIONAL_INDICATOR;
  if (Is_Emoji_Keycap_Base (codepoint))
    return KEYCAP_BASE;
  if (Is_Emoji_Emoji_Default (codepoint))
    return EMOJI_EMOJI_PRESENTATION;
  if (Is_Emoji (codepoint))
    /* return EMOJI; */
    return EMOJI_TEXT_PRESENTATION;

  /* Ragel state machine will interpret unknown category as "any". */
  return MaxEmojiScannerCategory;
}


typedef uint32_t *emoji_text_iter_t;
#include "emoji_presentation_scanner.c"

emoji_text_iter_t itemize_emoji(emoji_text_iter_t str, size_t len, bool *start_emoji) {
  emoji_text_iter_t out = scan_emoji_presentation(str, str + len, start_emoji);// - iter->types;

  bool is_emoji = *start_emoji;
  while (out < (str + len)){
    out = scan_emoji_presentation(out, str + len, &is_emoji);
    if (is_emoji != *start_emoji) {
      --out ;
      break;
    }
    /* out = r; */
  }
  return out;
}
