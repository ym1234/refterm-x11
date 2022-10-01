// gcc shape.c -g -fsanitize=undefined -fsanitize=leak -fsanitize=address -lfribidi -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/fribidi
#include <hb.h>
#include <fribidi.h>
#include <stdbool.h>
#include <hb-ft.h>
#include <fontconfig/fontconfig.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_LCD_FILTER_H

void die(char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	/* exit(-1); */
}
#define FT_ASSERT(x) \
	do {\
		FT_Error error;\
		if (error = (x)) {\
			die("%s:%d 0x%x, %s\n", __FILE__, __LINE__, error, FT_Error_String(error));\
		}\
	} while (0);


#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

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

#define FONT_SIZE 30
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

	/* FT_New_Face(ft_library, "/usr/share/fonts/noto/NotoSansArabic-Regular.ttf", 0, f); */
	if (FT_New_Face(ft_library, name, 0, f))
		abort();
	if (FT_Set_Pixel_Sizes(*f, 0, FONT_SIZE)) {
		for (int i = 0; i < (*f)->num_fixed_sizes; ++i) {
			FT_Bitmap_Size size = (*f)->available_sizes[i];
			printf("height: %d, width: %d\n", size.height, size.width);
		}
		FT_Select_Size(*f, 0);
		/* die(""); */
	}
	/* if (FT_Set_Char_Size(*f, FONT_SIZE*64, FONT_SIZE*64, 0, 0)) */
		/* abort(); */
}

typedef struct {
	int width, height, channels;
	int glyph_width, glyph_height;
	int glyph_num;
	int x, y;
	int descent, ascent;
	char *buf;
	bool init;
} drawing_context;

static drawing_context dc;

void draw(drawing_context* dc, hb_buffer_t *hb_buffer, hb_font_t *hb_font) {
	FT_Face face = hb_ft_font_get_face(hb_font);
	if (!dc->init) {
		drawing_context c = {
			.width = 6 * 20 * 10.5,
			.height = 35,
			.x = 0,
			.y = 0,
			.ascent = face->size->metrics.ascender >> 6,
			.descent = -face->size->metrics.descender >> 6,
			.glyph_height = c.ascent + c.descent,
			.glyph_width = 0,
			.buf = malloc(c.width * c.height * 3 * sizeof(char)),
			.init = true,
		};
		/* memset(c.buf, 255, c.width * c.height * 3); */
		*dc = c;
	}

	int len = hb_buffer_get_length(hb_buffer);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);

	int offset = 0;
	for (unsigned int i = 0; i < len; i++) {
		hb_codepoint_t gid   = info[i].codepoint;
		hb_glyph_position_t p = pos[i];
		int top;
		/* printf("has_color: %d", FT_HAS_COLOR(face)); */
		/* if (FT_HAS_COLOR(face)) { */
		/* 	FT_ASSERT(FT_Load_Glyph(face, gid, FT_LOAD_COLOR | FT_LOAD_DEFAULT)); */
		/* 	FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); */
		/* 	top = 0; */
		/* } else { */
			FT_ASSERT(FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT));
			/* FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); */
			FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); // FT_RENDER_MODE_NORMAL)); // FT_RENDER_MODE_LCD));
			printf("descent: %d\n", dc->descent);
			printf("bitmap_top: %d\n", face->glyph->bitmap_top);
			top = 7 + FONT_SIZE - dc->descent - face->glyph->bitmap_top;
			printf("top: %d\n", top);
			if (top < 0)  {
				top = 0;
			}
		/* } */
		FT_Bitmap bitmap = face->glyph->bitmap;

		/* printf("%d */

		printf("bitmap_top: %d\n", face->glyph->bitmap_top);
		int left = 3 * (round(dc->x / 64.) + face->glyph->bitmap_left + offset);
		if (left < 0) {
			/* printf("Here"); */
			dc->x += abs(face->glyph->bitmap_left) * 64.;
			left = round(dc->x / 64.);
		}


		for (int l = 0; l < bitmap.rows; l++) {
			int idx = left + 3 * dc->width * (top + l);
			for (int z = 1; z < bitmap.width/3; ++z) {

				/* switch (bitmap.pixel_mode){ */
					/* case 7: { */
					/* 	// BGRA -> RGBA */
					/* 	int k = z * 4; */
					/* 	dc->buf[idx++] = (char) bitmap.buffer[l * bitmap.pitch + k + 2]; */
					/* 	dc->buf[idx++] = (char) bitmap.buffer[l * bitmap.pitch + k + 1]; */
					/* 	dc->buf[idx++] = (char) bitmap.buffer[l * bitmap.pitch + k]; */
					/* 	dc->buf[idx++] = (char) bitmap.buffer[l * bitmap.pitch + k + 3]; */
					/* } break; */
					/* case 5: { */
						/* if (idx % 4 == 3) dc->buf[idx++] = 1; */
				/* dc->buf[idx++] = bitmap.buffer[l * bitmap.pitch + z]; */
				int k = z*3;
				float FR = 255, FG = 255 , FB = 255;
				float BR = 0, BG = 0, BB = 0;
				float R = bitmap.buffer[l * bitmap.pitch + k + 0] / 255.;
				float G = bitmap.buffer[l * bitmap.pitch + k + 1] / 255.;
				float B = bitmap.buffer[l * bitmap.pitch + k + 2] / 255.;
				/* if(!dc->buf[idx]) { */
					dc->buf[idx + 0] = (char) ((1. - R) * BR + FR * R);
					dc->buf[idx + 1] = (char) ((1. - G) * BG + FG * G);
					dc->buf[idx + 2] = (char) ((1. - B) * (float) BB + (float) FB * B);
				/* } */
					idx += 3;
				/* dc->buf[idx++] =  255 - bitmap.buffer[l * bitmap.pitch + z]; */
						/* dc->buf[idx + 0] = dc->buf[idx + 0] + bitmap.buffer[l * bitmap.pitch + z]; */
						/* dc->buf[idx + 1] = dc->buf[idx + 1] + bitmap.buffer[l * bitmap.pitch + z]; */
						/* dc->buf[idx + 2] = dc->buf[idx + 2] + bitmap.buffer[l * bitmap.pitch + z]; */
						/* idx += 1; */
					/* } break; */
				/* } */
			}
		}

		printf("gid: %d, Bitmap: rows: %d, width: %d, pitch: %d, pixel_mode: %d, bitmap_left %d, bitmap_top: %d, x_advance: %d\n", gid, face->glyph->bitmap.rows, face->glyph->bitmap.width, face->glyph->bitmap.pitch, face->glyph->bitmap.pixel_mode, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->advance.x);
		unsigned int cluster = info[i].cluster;
		dc->x += p.x_advance; // face->glyph->advance.x >> 6;
	}
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
		double x_advance = pos[i].x_advance;
		double y_advance = pos[i].y_advance;
		double x_offset  = pos[i].x_offset;
		double y_offset  = pos[i].y_offset;

		char glyphname[32];
		hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

		printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g), gid=%x\n",
				glyphname, cluster, x_advance, y_advance, x_offset, y_offset, gid);
	}

	draw(&dc, hb_buffer, hb_font);
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
	/* hb_language_t lang = NULL; */

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

		// TODO(ym): japanese
		/* if (script == HB_SCRIPT_HANI) { */
		/* 	if (idxs == HB_SCRIPT_HIRAGANA || idxs == HB_SCRIPT_KATAKANA) { */
		/* 		lang = */
		/* 	} */
		/* } */

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

int main(int argc, char *argv[]) {
	/* if (argc < 2) { */
	/* 	printf("Please run me with more than 0 arguments"); */
	/* 	return -1; */
	/* } */
	/* const char *str = argv[1]; */
	FT_Init_FreeType (&ft_library);
	/* FT_Library_SetLcdFilter(ft_library, FT_LCD_FILTER_DEFAULT); */
	/* char str[] = "a"; */
	/* char str[] = "\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab"; */
	/* char str[] = "\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab"; */
	/* char str[] = "الس\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab"; */
	/* char str[] = "hello world"; */
	/* char str[] = "\xe1\x84\x82\xe1\x85\xa1\xe1\x84\x82\xe1\x85\xb3\xe1\x86\xab \xe1\x84\x92\xe1\x85\xa5\xe1\x86\xab\xe1\x84\x87\xe1\x85\xa5\xe1\x86\xb8\xe1\x84\x8b\xe1\x85\xb3\xe1\x86\xaf \xe1\x84\x8c\xe1\x85\xae\xe1\x86\xab\xe1\x84\x89\xe1\x85\xae\xe1\x84\x92\xe1\x85\xa1\xe1\x84\x80\xe1\x85\xa9 \xe1\x84\x80\xe1\x85\xae\xe1\x86\xa8\xe1\x84\x80\xe1\x85\xa1\xe1\x84\x85\xe1\x85\xb3\xe1\x86\xaf"; */
	/* char str[] = "Hello world (السًٌَُلام عليكم\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab Hello world 日本語"; */
	/* char str[] = "Hd"; */
	char str[] = "he said “\xE2\x81\xA7 お前の名前は？日本語 car تعنًَُى عًٌَُربى\xE2\x81\xA9.” “\xE2\x81\xA7نعم\xE2\x81\xA9,” she agreed."
		"\xe1\x84\x82\xe1\x85\xa1\xe1\x84\x82\xe1\x85\xb3\xe1\x86\xab \xe1\x84\x92\xe1\x85\xa5\xe1\x86\xab\xe1\x84\x87\xe1\x85\xa5\xe1\x86\xb8\xe1\x84\x8b\xe1\x85\xb3\xe1\x86\xaf \xe1\x84\x8c\xe1\x85\xae\xe1\x86\xab\xe1\x84\x89\xe1\x85\xae\xe1\x84\x92\xe1\x85\xa1\xe1\x84\x80\xe1\x85\xa9 \xe1\x84\x80\xe1\x85\xae\xe1\x86\xa8\xe1\x84\x80\xe1\x85\xa1\xe1\x84\x85\xe1\x85\xb3\xe1\x86\xaf";
	/* char str[] =  "\xE2\x81\xA7 السلام عليكم ’\xE2\x81\xA6 he said “\xE2\x81\xA7 car MEANS CAR=”\xE2\x81\xA9‘?"; */

	/* char *str = "السلام "; */
	/* int len = sizeof(str) - 1; */
	int len = strlen(str);
	printf("len: %d\n", len);
	int leftover = 0;
	really_really_shape(str, len, &leftover);
	stbi_write_png("idk.png", dc.width, dc.height, 3, dc.buf, dc.width * 3);
}
