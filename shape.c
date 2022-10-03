// gcc shape.c -g -fsanitize=undefined -fsanitize=leak -fsanitize=address -lfribidi -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/fribidi

#include <hb.h>
#include <fribidi.h>
#include <stdbool.h>
#include <hb-ft.h>
#include <fontconfig/fontconfig.h>
#include <math.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_LCD_FILTER_H

#include "bicubic_scaling.c"

void die(char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	/* exit(-1); */
}
#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif
#ifndef MIN
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#endif
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

#define FONT_SIZE 300
double find_font(FT_Library ft_library, FT_Face *f, hb_language_t language, unsigned int *utf32, int len) {
	if (language == NULL) printf("Language is nuLl\n");
	FcPattern *p = FcPatternCreate(); // TODO(ym): Destroy this later
	FcCharSet *s = FcCharSetCreate();
	FcLangSet *l = FcLangSetCreate();

	const char *lang_tag = hb_language_to_string(language);
	printf("Language %s\n", lang_tag);
	FcLangSetAdd(l, lang_tag); // This doesn't seem to be working, it just returns c?;
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
	double pixelfixup = 1;
	FcPatternGetDouble(m, "pixelsizefixupfactor", 0, &pixelfixup);
	printf("%s\n", name);
	printf("pixelfixup: %f\n", pixelfixup);

	/* if (!strcmp("/usr/share/fonts/noto/NotoNaskhArabic-Regular.ttf", name)) { */
	/* 	name = "/usr/share/fonts/noto/NotoSansArabic-Regular.ttf"; //"/usr/share/fonts/noto/NotoSerifDevanagari-Regular.ttf"; */
	/* } */
	/* if (!strcmp("/usr/share/fonts/noto/NotoSerifDevanagari-Regular.ttf" */
	if (FT_New_Face(ft_library, name, 0, f))
		abort();
	if (FT_Set_Pixel_Sizes(*f, 0, FONT_SIZE)) {
		for (int i = 0; i < (*f)->num_fixed_sizes; ++i) {
			FT_Bitmap_Size size = (*f)->available_sizes[i];
			printf("height: %d, width: %d\n", size.height, size.width);
		}
		FT_Select_Size(*f, 0);
	}

	FcMatrix *fc_matrix;
	bool has_matrix = FcPatternGetMatrix(m, FC_MATRIX, 0, &fc_matrix) == FcResultMatch;
	printf("Has matrix: %d\n",has_matrix );
	if (has_matrix) {
		FT_Matrix m = {
			.xx = fc_matrix->xx * 0x10000,
			.xy = fc_matrix->xy * 0x10000,
			.yx = fc_matrix->yx * 0x10000,
			.yy = fc_matrix->yy * 0x10000,
		};
		FT_Set_Transform(*f, &m, NULL); // Doesn't seem to do anything?
	}
	return 0.10; // pixelfixup;
	/* if (FT_Set_Char_Size(*f, FONT_SIZE*64, FONT_SIZE*64, 0, 0)) */
		/* abort(); */
}

typedef struct {
	int width, height, channels;
	int glyph_width, glyph_height;
	int glyph_num;
	int x, y;
	int descent, ascent;
	unsigned char *buf;
	bool init;
} drawing_context;

static drawing_context dc;

void draw(drawing_context* dc, hb_buffer_t *hb_buffer, double scale, hb_font_t *hb_font) {
	FT_Face face = hb_ft_font_get_face(hb_font);
	if (!dc->init) {
		drawing_context c = {
			.width = 6 * 20 * 15,
			.height = 300,
			.x = 0,
			.y = 0,
			.ascent = round(face->size->metrics.ascender / 64.),
			.descent = -round(face->size->metrics.descender / 64.),
			.glyph_height = c.ascent + c.descent,
			.glyph_width = 0,
			.buf = malloc(c.width * c.height * 4 * sizeof(char)),
			.init = true,
		};
		c.glyph_height = c.ascent + c.descent;
		memset(c.buf, 255, c.width * c.height * 4);
		*dc = c;
		/* dc->x = 1; */
	}

	int len = hb_buffer_get_length(hb_buffer);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);

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
		int row_start = 0;
			FT_ASSERT(FT_Load_Glyph(face, gid, FT_LOAD_COLOR | FT_LOAD_DEFAULT));
			/* FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); */
			FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); // FT_RENDER_MODE_NORMAL)); // FT_RENDER_MODE_LCD));
			printf("glyph_height: %d\n", dc->glyph_height);
			top = dc->glyph_height + 1 - dc->descent - face->glyph->bitmap_top - round(p.y_offset/ 64.);
			// Fix this?
			if (top < 0)  {
				printf("top: %d\n", top);
				row_start = abs(top);
				top = 0;
			}
		/* } */
		FT_Bitmap bitmap = face->glyph->bitmap;


		int left = 4 * (round((dc->x + p.x_offset) / 64.) + face->glyph->bitmap_left);
		// Test this
		if (left < 0) {
			left = 4 * round(dc->x / 64.);
			dc->x += abs(p.x_offset + face->glyph->bitmap_left);
		}

		char *buf = bitmap.buffer;
		int h = (float) bitmap.rows * scale;
		int w = (float) bitmap.width * scale;
		int pitch = (float) bitmap.pitch * scale;
		/* if (scale != 1) { */
			buf = bicubic_scaling(bitmap.buffer, bitmap.width, bitmap.rows, bitmap.pitch / 4, scale);
		/* } */
		printf("h: %d, w: %d, pitch: %d, original_pitch: %d\n", h, w, pitch, bitmap.pitch);

		for (int l = row_start; l < h; l++) {
			int idx = left + 4 * dc->width * (top + l - row_start);
			for (int z = 0; z < w; ++z) {
				switch (bitmap.pixel_mode) {
					case 7: {
						// BGRA -> RGBA
						int k = z * 4;
						dc->buf[idx++] = (char) buf[4 * l * w + k + 2];
						dc->buf[idx++] = (char) buf[4 * l * w + k + 1];
						dc->buf[idx++] = (char) buf[4 * l * w + k];
						dc->buf[idx++] = (char) buf[4 * l * w + k + 3];
					} break;
					case 5: {
						if (idx % 4 == 3) dc->buf[idx++] = 1;
						dc->buf[idx++] =  255 - bitmap.buffer[l * bitmap.pitch + z];
					} break;
				}
			}
		}

		/* stbi_write_png("idk.png", dc->width, dc->height, 3, dc->buf, dc->width * 3); */
		printf("gid: %d, Bitmap: rows: %d, width: %d, pitch: %d, pixel_mode: %d, bitmap_left %d, bitmap_top: %d, x_advance: %d\n", gid, face->glyph->bitmap.rows, face->glyph->bitmap.width, face->glyph->bitmap.pitch, face->glyph->bitmap.pixel_mode, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->advance.x);
		unsigned int cluster = info[i].cluster;
		dc->x += p.x_advance * scale; // face->glyph->advance.x >> 6;
	}
}


void shape_really(FT_Face fc, double pixelfixup, hb_buffer_t *hb_buffer) {
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

	draw(&dc, hb_buffer, pixelfixup, hb_font);
	hb_font_destroy (hb_font);
}


void shape_really_really_really_now(hb_buffer_t *hb_buffer, hb_script_t script, uint32_t *buf, uint32_t len, uint32_t start, uint32_t seqlen) {
	hb_buffer_add_codepoints(hb_buffer, buf, len, start, seqlen);
	hb_buffer_set_script(hb_buffer, script);
	hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR); //level % 2 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_guess_segment_properties(hb_buffer);
	FT_Face fc;
	hb_language_t lang = hb_buffer_get_language(hb_buffer);
	double r = find_font(ft_library, &fc, lang, buf  + start, seqlen);
	shape_really(fc, r, hb_buffer);
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
		if (/*script == HB_SCRIPT_COMMON ||  */script == HB_SCRIPT_INHERITED /* || idxs == HB_SCRIPT_UNKNOWN */) script = idxs;
		if (/*idxs == HB_SCRIPT_COMMON || */ idxs == HB_SCRIPT_INHERITED /* || idxs == HB_SCRIPT_UNKNOWN */) idxs = script;

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
	char str[] = "\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\xa4\xb9\xf0\x9f\x8f\xbe\xe2\x80\x8d\xe2\x99\x80\xef\xb8\x8f\xf0\x9f\x91\xa8\xf0\x9f\x8f\xbc\xe2\x80\x8d\xf0\x9f\x9a\x80";
	/* char str[] = "الس\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab"; */
	/* char str[] = "hello world"; */
	/* char str[] = "\xe1\x84\x82\xe1\x85\xa1\xe1\x84\x82\xe1\x85\xb3\xe1\x86\xab \xe1\x84\x92\xe1\x85\xa5\xe1\x86\xab\xe1\x84\x87\xe1\x85\xa5\xe1\x86\xb8\xe1\x84\x8b\xe1\x85\xb3\xe1\x86\xaf \xe1\x84\x8c\xe1\x85\xae\xe1\x86\xab\xe1\x84\x89\xe1\x85\xae\xe1\x84\x92\xe1\x85\xa1\xe1\x84\x80\xe1\x85\xa9 \xe1\x84\x80\xe1\x85\xae\xe1\x86\xa8\xe1\x84\x80\xe1\x85\xa1\xe1\x84\x85\xe1\x85\xb3\xe1\x86\xaf"; */
	/* char str[] = "Hello world (السًٌَُلام عليكم\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab Hello world 日本語"; */
	/* char str[] = "Hd"; */
	/* char str[] = "he said “\xE2\x81\xA7 お前の名前は？日本語 car تعنًَُى عًٌَُربى\xE2\x81\xA9.” “\xE2\x81\xA7نعم\xE2\x81\xA9,” she agreed."; */
	/* char str[] = "Ленивый рыжий кот شَدَّة latin العَرَبِية"; */
	/* char str[] = "ﷺ نَهَضْتُ مِنْ مَكَانِي وأَنَا أَشْعُر بِنَشَاطٍ غَرِيب وكَأنّ نَبْعاً مِن القُوّةِ الدَاخِلِيّةِ قَدْ سَرَى فِي جَسَدِي.."; */
	/* char str[] = "\xe0\xb8\xa1\xe0\xb8\xb5\xe0\xb8\xab\xe0\xb8\xa5\xe0\xb8\xb1\xe0\xb8\x81\xe0\xb8\x90\xe0\xb8\xb2\xe0\xb8\x99\xe0\xb8\x97\xe0\xb8\xb5\xe0\xb9\x88\xe0\xb9\x80\xe0\xb8\x9b\xe0\xb9\x87\xe0\xb8\x99\xe0\xb8\x82\xe0\xb9\x89\xe0\xb8\xad\xe0\xb9\x80\xe0\xb8\x97\xe0\xb9\x87\xe0\xb8\x88\xe0\xb8\x88\xe0\xb8\xa3\xe0\xb8\xb4\xe0\xb8\x87\xe0\xb8\xa2\xe0\xb8\xb7\xe0\xb8\x99\xe0\xb8\xa2\xe0\xb8\xb1\xe0\xb8\x99\xe0\xb8\xa1\xe0\xb8\xb2\xe0\xb8\x99\xe0\xb8\xb2\xe0\xb8\x99\xe0\xb9\x81\xe0\xb8\xa5\xe0\xb9\x89\xe0\xb8\xa7 \xe0\xb8\xa7\xe0\xb9\x88\xe0\xb8\xb2\xe0\xb9\x80\xe0\xb8\x99\xe0\xb8\xb7\xe0\xb9\x89\xe0\xb8\xad\xe0\xb8\xab\xe0\xb8\xb2\xe0\xb8\x97\xe0\xb8\xb5\xe0\xb9\x88\xe0\xb8\xad\xe0\xb9\x88\xe0\xb8\xb2\xe0\xb8\x99\xe0\xb8\xa3\xe0\xb8\xb9\xe0\xb9\x89\xe0\xb9\x80\xe0\xb8\xa3\xe0\xb8\xb7\xe0\xb9\x88\xe0\xb8\xad\xe0\xb8\x87\xe0\xb8\x99\xe0\xb8\xb1\xe0\xb9\x89\xe0\xb8\x99\xe0\xb8\x88\xe0\xb8\xb0\xe0\xb9\x84\xe0\xb8\x9b\xe0\xb8\x81\xe0\xb8\xa7\xe0\xb8\x99\xe0\xb8\xaa\xe0\xb8\xa1\xe0\xb8\xb2\xe0\xb8\x98\xe0\xb8\xb4\xe0\xb8\x82\xe0\xb8\xad\xe0\xb8\x87\xe0\xb8\x84\xe0\xb8\x99\xe0\xb8\xad\xe0\xb9\x88\xe0\xb8\xb2\xe0\xb8\x99\xe0\xb9\x83\xe0\xb8\xab\xe0\xb9\x89\xe0\xb9\x80\xe0\xb8\x82\xe0\xb8\xa7\xe0\xb9\x84\xe0\xb8\x9b\xe0\xb8\x88\xe0\xb8\xb2\xe0\xb8\x81\xe0\xb8\xaa\xe0\xb9\x88\xe0\xb8\xa7\xe0\xb8\x99\xe0\xb8\x97\xe0\xb8\xb5\xe0\xb9\x89\xe0\xb9\x80\xe0\xb8\x9b\xe0\xb9\x87\xe0\xb8\x99"; */
	/* 	"\xe1\x84\x82\xe1\x85\xa1\xe1\x84\x82\xe1\x85\xb3\xe1\x86\xab \xe1\x84\x92\xe1\x85\xa5\xe1\x86\xab\xe1\x84\x87\xe1\x85\xa5\xe1\x86\xb8\xe1\x84\x8b\xe1\x85\xb3\xe1\x86\xaf \xe1\x84\x8c\xe1\x85\xae\xe1\x86\xab\xe1\x84\x89\xe1\x85\xae\xe1\x84\x92\xe1\x85\xa1\xe1\x84\x80\xe1\x85\xa9 \xe1\x84\x80\xe1\x85\xae\xe1\x86\xa8\xe1\x84\x80\xe1\x85\xa1\xe1\x84\x85\xe1\x85\xb3\xe1\x86\xaf"; */
	/* char str[] = "A\xcc\x81 A\xcc\x81 A\xcc\x81 A\xcc\x81 A\xcc\x81"; */
	/* char str[] =  "\xE2\x81\xA7 السلام عليكم ’\xE2\x81\xA6 he said “\xE2\x81\xA7 car MEANS CAR=”\xE2\x81\xA9‘?"; */

	/* char str[] = "Hello world شَدَّة العَرَبِية はたらく魔王さま！"; */
	/* char str[] = "Lorem Ipsum HEllo world"; */
	/* char str[] = "\xd7\x91\xd7\x94 \xd7\xa1\xd7\xa4\xd7\xa8\xd7\x95\xd7\xaa \xd7\xaa\xd7\xa7\xd7\xa9\xd7\x95\xd7\xa8\xd7\xaa \xd7\x9c\xd7\xa2\xd7\x91\xd7\xa8\xd7\x99\xd7\xaa \xd7\xaa\xd7\xa0\xd7\x9a, \xd7\x92\xd7\x9d"; */
	/* char str[] = "\xd7\x97\xd7\x95\xd6\xb9\xd7\x9c\xd6\xb8\xd7\x9d \xd7\x9e\xd6\xb8\xd7\x9c\xd6\xb5\xd7\x90 \xd7\xa6\xd6\xb5\xd7\x99\xd7\xa8\xd6\xb6\xd7\x94 \xd7\x9e\xd6\xb8\xd7\x9c\xd6\xb5\xd7\x90 \xd7\xa4\xd6\xb7\xd6\xbc\xd7\xaa\xd6\xb8\xd6\xbc\xd7\x97  קֻבּוּץ"; */
	/* char str[] = "\xe0\xa4\xb9\xe0\xa4\xbe\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xa1\xe0\xa4\xb5\xe0\xa5\x87\xe0\xa4\xb0 \xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\x9a\xe0\xa4\xb0\xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\xae\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xb6 \xe0\xa4\x86\xe0\xa4\xaa\xe0\xa4\x95\xe0\xa5\x8b \xe0\xa4\xaa\xe0\xa4\xb9\xe0\xa5\x8b\xe0\xa4\x9a"; */

	/* char str[]="\xd8\xa7\xd9\x84\xd8\xad\xd9\x85\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd8\xaf"; */
	/* char str[] = "\xd8\xb1\xd8\xad\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd9\x80\xd9\x8a\xd9\x85"; */
	/* char str[] = "الســــلام"; */
	/* char str[] = "मराठी लिꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻꣻपी"; */

	/* char str[] = "\xd6\xb0\xd7\x82\xd6\xa9\xd7\xb2\xd6\xa5\xd6\xa4\xd7\xa0\xd6\xb9\xd6\x9a\xd6\xa9\xd7\xa2\xd7\x99\xd6\xb4\xd6\xa6\xd6\xa5\xd6\x9f\xd6\x98\xd7\xb2\xd6\xb7\xd7\x9f\xd6\xa7\xd7\xa5\xd7\x9d\xd7\xaa\xd6\xb1\xd6\x91"; */

	/* char str[] = "\xe0\xa4\xb9\xe0\xa4\xbe\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xa1\xe0\xa4\xb5\xe0\xa5\x87\xe0\xa4\xb0 \xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\x9a\xe0\xa4\xb0\xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\xae\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xb6 \xe0\xa4\x86\xe0\xa4\xaa\xe0\xa4\x95\xe0\xa5\x8b \xe0\xa4\xaa\xe0\xa4\xb9\xe0\xa5\x8b\xe0\xa4\x9a"; */

	/* char str[] = " अआइईउऊ"; */
	/* char str[] = "हार्डवेर विचरविमर्श आपको पहोच।"; */
	/* char str[] = "\xe0\xa4\xb9\xe0\xa4\xbe\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xa1\xe0\xa4\xb5\xe0\xa5\x87\xe0\xa4\xb0 \xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\x9a\xe0\xa4\xb0\xe0\xa4\xb5\xe0\xa4\xbf\xe0\xa4\xae\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\xb6 \xe0\xa4\x86\xe0\xa4\xaa\xe0\xa4\x95\xe0\xa5\x8b \xe0\xa4\xaa\xe0\xa4\xb9\xe0\xa5\x8b\xe0\xa4\x9a"; */

	/* char *str = "السلام "; */
	/* char *str = "...the music melt̸s͝ in͞ S̷ib͘e̵l̵i͡u̡s̀."; */
	/* char str[] = "...the music melt\xcc\xb8s\xcd\x9d in\xcd\x9e S\xcc\xb7ib\xcd\x98e\xcc\xb5l\xcc\xb5i\xcd\xa1u\xcc\xa1s\xcc\x80."; */
	/* int len = sizeof(str) - 1; */
	/* char *str =" ꜱ̯̤ɪ̫͙ʙᴇ̻ʟ͖ɪ͙͚ᴜ̪̠̝͕̼͓ꜱ̼"; */
	/* char *str ="ꜱ͘ɪʙ̢ᴇ҉ʟ͞ɪ͟ᴜ͡ꜱ̶ ̸ᴄʀ̕ᴀ̢ꜱ҉ʜ̶ᴇ͞ᴅ!͏"; */


	/* char *str = "\xe0\xbc\x8d\xe0\xbd\x85\xe0\xbd\x82\xe0\xbd\xa6\xe0\xbc\x8b\xe0\xbd\x82\xe0\xbd\xa2\xe0\xbe\xa8\xe0\xbd\xba\xe0\xbd\xa6\xe0\xbd\x91\xe0\xbc\x8b\xe0\xbd\x96\xe0\xbd\xa1\xe0\xbd\xa3\xe0\xbd\xa6\xe0\xbc\x8b\xe0\xbd\x96\xe0\xbd\x96\xe0\xbd\x93\xe0\xbc\x8b\xe0\xbd\x96\xe0\xbd\xa6\xe0\xbe\x92\xe0\xbd\xbc\xe0\xbd\x84\xe0\xbd\xa6\xe0\xbc\x8b\xe0\xbd\xa0\xe0\xbd\xa3\xe0\xbe\x90\xe0\xbd\xba\xe0\xbd\xa0\xe0\xbd\xa6\xe0\xbc\x8b\xe0\xbd\x91\xe0\xbd\xa2\xe0\xbe\x97\xe0\xbd\xba\xe0\xbd\x82\xe0\xbc\x8b"; */
	/* char *str = "ပါဠိစာအမှန် (၁၂) ကြောင်း နှင့် မြန်မာ စာအမှန် (၁၂) ကြောင်း၏"; */

	/* char *str = "\xe1\x9e\x98\xe1\x9e\xb6\xe1\x9e\x8f\xe1\x9f\x92\xe1\x9e\x9a\xe1\x9e\xb6 \xe1\x9f\xa1 \xe1\x9e\x98\xe1\x9e\x93\xe1\x9e\xbb\xe1\x9e\x9f\xe1\x9f\x92\xe1\x9e\x9f\xe1\x9e\x91\xe1\x9e\xb6\xe1\x9f\x86\xe1\x9e\x84\xe1\x9e\xa2\xe1\x9e\x9f\xe1\x9f\x8b \xe1\x9e\x80\xe1\x9e\xbe\xe1\x9e\x8f\xe1\x9e\x98\xe1\x9e\x80\xe1\x9e\x98\xe1\x9e\xb6\xe1\x9e\x93\xe1\x9e\x9f\xe1\x9f\x81\xe1\x9e\x9a\xe1\x9e\xb8\xe1\x9e\x97\xe1\x9e\xb6\xe1\x9e\x96 \xe1\x9e\x93\xe1\x9e\xb7\xe1\x9e\x84\xe1\x9e\x9f\xe1\x9e\x98\xe1\x9e\x97\xe1\x9e\xb6\xe1\x9e\x96 \xe1\x9e\x80\xe1\x9f\x92\xe1\x9e\x93\xe1\x9e\xbb\xe1\x9e\x84\xe1\x9e\x95\xe1\x9f\x92\xe1\x9e\x93\xe1\x9f\x82\xe1\x9e\x80\xe1\x9e\x9f\xe1\x9f\x81\xe1\x9e\x85\xe1\x9e\x80\xe1\x9f\x92\xe1\x9e\x8a\xe1\x9e\xb8\xe1\x9e\x90\xe1\x9f\x92\xe1\x9e\x9b\xe1\x9f\x83\xe1\x9e\x90\xe1\x9f\x92\xe1\x9e\x93\xe1\x9e\xbc\xe1\x9e\x9a\xe1\x9e\x93\xe1\x9e\xb7\xe1\x9e\x84\xe1\x9e\x9f\xe1\x9e\xb7\xe1\x9e\x91\xe1\x9f\x92\xe1\x9e\x92\xe1\x9e\xb7"; */
	int len = strlen(str);

	/* setlocale( */
	printf("len: %d\n", len);
	int leftover = 0;
	hb_language_t default_language = hb_language_get_default();
	printf("Default language: %s\n", hb_language_to_string(default_language));
	really_really_shape(str, len, &leftover);
	stbi_write_png("idk.png", dc.width, dc.height, 4, dc.buf, dc.width * 4);
	return 0;
}
