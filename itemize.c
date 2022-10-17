#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

static const uint32_t paired_chars[] = {
  0x0028, 0x0029, /* ascii paired punctuation */
  0x003c, 0x003e,
  0x005b, 0x005d,
  0x007b, 0x007d,
  0x00ab, 0x00bb, /* guillemets */
  0x0f3a, 0x0f3b, /* tibetan */
  0x0f3c, 0x0f3d,
  0x169b, 0x169c, /* ogham */
  0x2018, 0x2019, /* general punctuation */
  0x201c, 0x201d,
  0x2039, 0x203a,
  0x2045, 0x2046,
  0x207d, 0x207e,
  0x208d, 0x208e,
  0x27e6, 0x27e7, /* math */
  0x27e8, 0x27e9,
  0x27ea, 0x27eb,
  0x27ec, 0x27ed,
  0x27ee, 0x27ef,
  0x2983, 0x2984,
  0x2985, 0x2986,
  0x2987, 0x2988,
  0x2989, 0x298a,
  0x298b, 0x298c,
  0x298d, 0x298e,
  0x298f, 0x2990,
  0x2991, 0x2992,
  0x2993, 0x2994,
  0x2995, 0x2996,
  0x2997, 0x2998,
  0x29fc, 0x29fd,
  0x2e02, 0x2e03,
  0x2e04, 0x2e05,
  0x2e09, 0x2e0a,
  0x2e0c, 0x2e0d,
  0x2e1c, 0x2e1d,
  0x2e20, 0x2e21,
  0x2e22, 0x2e23,
  0x2e24, 0x2e25,
  0x2e26, 0x2e27,
  0x2e28, 0x2e29,
  0x3008, 0x3009, /* chinese paired punctuation */
  0x300a, 0x300b,
  0x300c, 0x300d,
  0x300e, 0x300f,
  0x3010, 0x3011,
  0x3014, 0x3015,
  0x3016, 0x3017,
  0x3018, 0x3019,
  0x301a, 0x301b,
  0xfe59, 0xfe5a,
  0xfe5b, 0xfe5c,
  0xfe5d, 0xfe5e,
  0xff08, 0xff09,
  0xff3b, 0xff3d,
  0xff5b, 0xff5d,
  0xff5f, 0xff60,
  0xff62, 0xff63
};

/* typedef struct { */
/* 	size_t PairIdx; */
/* } PairEntry; */

/* #define SCRIPT_STACK_DEPTH 128 */
/* typedef struct { */
/* 	uint32_t *str; */
/* 	size_t str_len; */
/* 	size_t stridx; */

/* 	PairEntry pairstack[SCIRPT_STACK_DEPTH]; */
/* 	size_t pairidx; */
/* } ScriptState; */


// NOTE(YM): defined in the freetype headers
/* ssize_t bsearch(uint32_t ch, uint32_t *arr, size_t len) { */
/*   size_t low = 0; */
/*   size_t high = len; */
/*   while (high > low) { */
/*     size_t mid = (low + high) / 2; */
/*     if (ch < arr[mid]) */
/*       high = mid - 1; */
/*     else if (ch > arr[mid]) */
/*       low = mid + 1; */
/*     else */
/*       return mid; */
/*   } */
/*   return -1; */
/* } */

#define REAL_SCRIPT(x) (x != HB_SCRIPT_INHERITED && x != HB_SCRIPT_COMMON && x != HB_SCRIPT_UNKNOWN)


// TODO(YM): Handle Parens, The Table is up there, but it was just too boring to handle
// Maybe I should just copy the code from pango...
//
//uint32_t *itemize_script(ScriptState *state) {
//	static hb_unicode_funcs_t *def = hb_unicode_funcs_get_default();
//  hb_script_t prev = HB_SCRIPT_COMMON;
//  uint32_t *i;
//  for (i = state.str + state.stridx; i < state.str + state.len; ++i) {
//    hb_scirpt_t script = hb_unicode_script(def, *i);
//    if (REAL_SCRIPT(script) && REAL_SCRIPT(prev) && script != prev) break;
//    if (REAL_SCRIPT(script)) prev = script;
//  }
//
//  state.stridx = i - state.str;
//  return i;
//}

uint32_t *itemize_script(uint32_t *str, size_t len, hb_script_t *sc) {
  static hb_unicode_funcs_t *unicodedefs;
  if (!unicodedefs) {
    unicodedefs = hb_unicode_funcs_get_default();
  }
  hb_script_t prev = HB_SCRIPT_COMMON;
  uint32_t *i = str;
  for (; i < str + len; ++i) {
    hb_script_t script = hb_unicode_script(unicodedefs, *i);
    if (REAL_SCRIPT(script) && REAL_SCRIPT(prev) && script != prev) break;
    if (likely(REAL_SCRIPT(script))) prev = script;
  }
  *sc = prev;
  return i;
}

FriBidiLevel *itemize_embeddings(FriBidiLevel *str, size_t len, FriBidiLevel *embedding) {
	if (!len) return str;
  *embedding = str[0];
	FriBidiLevel *end = str + len;
	while (++str < end && *embedding == *str);
	return str;
}

// TODO(YM): Do I need like a width itemizer? Doesn't sound like the type of thing i'd need
