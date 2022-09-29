// gcc shape.c -g -fsanitize=undefined -fsanitize=leak -fsanitize=address -lfribidi -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/fribidi
#include <hb.h>
#include <fribidi.h>
#include <stdbool.h>
#include <hb-ft.h>
#include <fontconfig/fontconfig.h>

// TODO(ym):
// - FontConfig Cache
// - Render with freetype
// - CJK Handling with maybe some config options
// - Lots and lots and lots of testing because this is just good awful
// - uuugh i hate this code
// - Oh yeah better names
static FT_Library ft_library;

typedef struct {
	hb_glyph_position_t *pos;
	hb_glyph_info_t *info;
	int offset;
	bool call;
} glyph_info;

typedef struct {
	int len;
	uint32_t *buf;
} utf32buf;

#define FONT_SIZE 12
void find_font(FT_Library ft_library, FT_Face *f, hb_language_t language, unsigned int *utf32, int len) {
	FcPattern *p = FcPatternCreate(); // TODO(ym): Destroy this later
	FcCharSet *s = FcCharSetCreate();
	FcLangSet *l = FcLangSetCreate();

	FcLangSetAdd(l, hb_language_to_string(language));
	for (int k = 0; k < len; k++) {
		FcCharSetAddChar(s, utf32[k]);
	}

	FcPatternAddDouble(p, FC_PIXEL_SIZE, FONT_SIZE);
	FcPatternAddCharSet(p, FC_CHARSET, s);
	FcPatternAddLangSet(p, FC_LANG, l);

	FcConfigSubstitute(NULL, p, FcMatchPattern);
	FcDefaultSubstitute(p);

	FcResult result;
	FcPattern *m = FcFontMatch(NULL, p, &result);

	unsigned char *name = NULL;
	FcPatternGetString(m, FC_FILE, 0, &name);
	printf("%s\n", name);

	/* FT_New_Face(ft_library, "/usr/share/fonts/noto/NotoColorEmoji.ttf", 0, f); */
	if (FT_New_Face(ft_library, name, 0, f))
		abort();
	/* if (FT_Set_Char_Size(*f, FONT_SIZE*64, FONT_SIZE*64, 0, 0)) */
		/* if(FT_Select_Size(face, 0)) */
		/* abort(); */
}

void shape_really(FT_Face fc, hb_buffer_t *hb_buffer) {
	static int x = 0;

	hb_font_t *hb_font;
	hb_font = hb_ft_font_create (fc, NULL);

	hb_shape (hb_font, hb_buffer, NULL, 0);

	int len = hb_buffer_get_length(hb_buffer);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);

	/* Print them out as is. */
	printf ("Raw buffer contents:\n");
	for (unsigned int i = 0; i < len; i++)
	{
		hb_codepoint_t gid   = info[i].codepoint;
		unsigned int cluster = info[i].cluster;
		double x_advance = pos[i].x_advance / 64.;
		double y_advance = pos[i].y_advance / 64.;
		double x_offset  = pos[i].x_offset / 64.;
		double y_offset  = pos[i].y_offset / 64.;

		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g), gid=%x\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset, gid);
	}

	hb_font_destroy (hb_font);
}


void shape_really_really_really_now(hb_buffer_t *hb_buffer, hb_script_t script, uint32_t *buf, uint32_t len, uint32_t start, uint32_t seqlen) {
	hb_buffer_add_codepoints(hb_buffer, buf, len, start, seqlen);
	hb_buffer_set_script(hb_buffer, script);
	hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR); //level % 2 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_guess_segment_properties(hb_buffer);
	FT_Face fc;
	find_font(ft_library, &fc, hb_buffer_get_language(hb_buffer), buf  + start, seqlen);
	shape_really(fc, hb_buffer);
}

void shape(uint32_t *str, int len, FriBidiLevel *levels, FriBidiStrIndex *V2L) {
	hb_buffer_t *hb_buffer = hb_buffer_create();
	hb_unicode_funcs_t *def = hb_unicode_funcs_get_default();
	hb_buffer_set_unicode_funcs(hb_buffer, def);

	int idx = 0;

	hb_script_t idxs = hb_unicode_script(def, str[idx]);
	FriBidiLevel level = levels[V2L[idx]];

	int i;
	for (i = 1; i < len; ++i) {
		hb_script_t script = hb_unicode_script(def, str[i]);
		if (levels[V2L[i]] != level) {
			printf("new level %d, old level: %d\n", levels[V2L[i]], level);
		}
		/* printf(" unicode general category: %d\n", hb_unicode_general_category(def, str.buf[i])); */
		char newscript[5] = {};
		hb_tag_to_string(hb_script_to_iso15924_tag(script), newscript);
		printf("newscript: %s\n", newscript);
		if (script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED /* || idxs == HB_SCRIPT_UNKNOWN */) script = idxs;
		if (idxs == HB_SCRIPT_COMMON || idxs == HB_SCRIPT_INHERITED /* || idxs == HB_SCRIPT_UNKNOWN */) idxs = script;

		if (script != idxs || levels[V2L[i]] != level) {
			shape_really_really_really_now(hb_buffer, idxs, str, len, idx, i - idx);
			char newscript[5] = {};
			char oldscript[5] = {};
			hb_tag_to_string(hb_script_to_iso15924_tag(script), newscript);
			hb_tag_to_string(hb_script_to_iso15924_tag(idxs), oldscript);
			printf("newscript: %s, oldscript: %s\n", newscript, oldscript);
			idx = i;
			idxs = script;
			level = levels[V2L[i]];
			hb_buffer_clear_contents(hb_buffer);
		}
	}

	if (i - idx){
		/* char newscript[5] = {}; */
		char oldscript[5] = {};
		/* hb_tag_to_string(hb_script_to_iso15924_tag(script), newscript); */
		hb_tag_to_string(hb_script_to_iso15924_tag(idxs), oldscript);
		printf("oldscript: %s\n", oldscript);
		printf("i = %d, idx = %d\n", i, idx);
		shape_really_really_really_now(hb_buffer, idxs, str, len, idx, i - idx);
	}
}

// TODO(ym): instead of repeating code, maybe make the buffer one character big with value -1/0?
glyph_info really_really_shape(char *str,  int len, int *leftover) {
	(void) leftover;
	uint32_t *buf = malloc(len * sizeof(*buf)),
	utf32len = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, str, len, buf);

	FriBidiParType base  =  FRIBIDI_PAR_ON;
	FriBidiLevel *levels = malloc(utf32len * sizeof(*levels));
	FriBidiChar *visual = malloc(utf32len * sizeof(*visual));
	FriBidiChar *logical = NULL; // malloc(utf32.len * sizeof(*logical));
	FriBidiStrIndex *V2L = malloc(utf32len * sizeof(*V2L));
	FriBidiLevel out = fribidi_log2vis(buf, utf32len, &base, visual, logical, V2L, levels);

	shape(visual, utf32len, levels, V2L);
}


int main(void) {
	FT_Init_FreeType (&ft_library);
	/* char str[] = "a"; */
	/* char str[] = "\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab"; */
	char str[] = "Hello world السلام عليكم\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab Hello world 日本語";
	/* char str[] = "he said “\xE2\x81\xA7 お前の名前は？日本語 car تعنًَُى عًٌَُربى\xE2\x81\xA9.” “\xE2\x81\xA7نعم\xE2\x81\xA9,” she agreed."; */
	/* char str[] =  "\xE2\x81\xA7 السلام عليكم ’\xE2\x81\xA6 he said “\xE2\x81\xA7 car MEANS CAR=”\xE2\x81\xA9‘?"; */
	int len = sizeof(str) - 1;
	printf("len: %d\n", len);
	int leftover = 0;
	really_really_shape(str, len, &leftover);
}
