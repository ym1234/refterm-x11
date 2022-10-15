// gcc shape.c -g -fsanitize=undefined -fsanitize=leak -fsanitize=address -lfribidi -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/fribidi
//
//
//
//
//
// What do we learn from all of this?
// Of course, that text is fucked up and no one should ever have to deal with it in their life, ever
//
//
#include <hb.h>
#include <fribidi.h>
#include <stdbool.h>
#include <hb-ft.h>
#include <fontconfig/fontconfig.h>
#include <math.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_LCD_FILTER_H

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

#include "scaling.c"
#include "itemize.c"
#include "emoji_scanner.c"

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
// - CJK Handling with maybe some config options
// - Lots and lots and lots of testing because this is just good awful


static FT_Library ft_library;
typedef struct {
	FT_Face f;
	int load_flags;
	int hintstyle;
	int pxsize;
	int ptsize;
} Font;

#define FONT_BUFFER_LENGTH 512 // 512 also doesn't seem too bad
typedef struct {
	Font main_font;
	/* glyph_table *CacheTable; */

	uint32_t str[FONT_BUFFER_LENGTH];
	uint32_t bidi_types[FONT_BUFFER_LENGTH];
	uint32_t bracket_types[FONT_BUFFER_LENGTH];
	uint32_t embedding_levels[FONT_BUFFER_LENGTH];

	hb_buffer_t *hb_buffer;
	GlyphSlot slot; // TODO(ym): support batch uploads, instead of the render-to-the-slot-then-upload approach
} FontSystem;

void print_buffer(hb_buffer_t *hb_buffer, hb_font_t *hb_font) {
	/* #if 0 */
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
	/* #endif */
}

void print_bidi(FriBidiLevel *levels, size_t len) {
	printf("\n");
	for (int i = 0; i < len; ++i) {
		printf("%d ", levels[i]);
	}
	printf("\n");
}

#define FONT_SIZE 30
double find_font(FT_Library ft_library, FT_Face *f, char *language, unsigned int *utf32, int len) {
	FcPattern *p = FcPatternCreate(); // TODO(ym): Destroy this later
	FcCharSet *s = FcCharSetCreate();
	FcLangSet *l = FcLangSetCreate();

	FcLangSetAdd(l, language);
	for (int k = 0; k < len; k++) {
		FcCharSetAddChar(s, utf32[k]);
	}

	FcPatternAddString(p, FC_STYLE, "monospace");
	/* FcPatternAddString(p, FC_SPACING, "monospace"); */
	FcPatternAddDouble(p, FC_PIXEL_SIZE, FONT_SIZE);
	FcPatternAddCharSet(p, FC_CHARSET, s);

	FcConfigSubstitute(NULL, p, FcMatchPattern);
	FcDefaultSubstitute(p);

	FcResult result;
	FcPattern *m = FcFontMatch(NULL, p, &result);

	unsigned char *name;
	FcPatternGetString(m, FC_FILE, 0, &name);
	double pixelfixup = 1;
	FcPatternGetDouble(m, "pixelsizefixupfactor", 0, &pixelfixup);

	if (FT_New_Face(ft_library, name, 0, f)) {
		die("Couldn't create a freetype face");
	}
	if (FT_Set_Char_Size(*f, FONT_SIZE*64, FONT_SIZE*64, 0, 0)) {
		/* if (FT_Set_Pixel_Sizes(*f, 0, FONT_SIZE)) { */
		for (int i = 0; i < (*f)->num_fixed_sizes; ++i) {
			FT_Bitmap_Size size = (*f)->available_sizes[i];
			printf("height: %d, width: %d\n", size.height, size.width);
		}
		FT_Select_Size(*f, 0);
	}

	FcMatrix *fc_matrix;
	if (FcPatternGetMatrix(m, FC_MATRIX, 0, &fc_matrix) == FcResultMatch) {
		FT_Matrix m = {
			.xx = fc_matrix->xx * 0x10000,
			.xy = fc_matrix->xy * 0x10000,
			.yx = fc_matrix->yx * 0x10000,
			.yy = fc_matrix->yy * 0x10000,
		};
		FT_Set_Transform(*f, &m, NULL); // Doesn't seem to do anything?
	}
	return pixelfixup;
}


// TODO(YM): These values are already linear, so only the (Fore|Back)Ground Values the TerminalCell sampler need to be gamma corrected
// TODO(YM): Check if emojis need gamma correction or not, aka whether they are *coverage* values or just plain colors
// TODO(YM): All this repeated code...
// They probably are plain colors, because they have an alpha component but ugh that's so annoying
static inline void copy5(GlyphSlot *dest, FT_Bitmap bitmap, long top, long left) {
	for (int i = 0; i < bitmap.rows; ++i) {
		for (int j = 0; j < bitmap.width/3; ++j) {
			float R = bitmap.buffer[i * bitmap.pitch + 3 * j + 0] / 255.;
			float G = bitmap.buffer[i * bitmap.pitch + 3 * j + 1] / 255.;
			float B = bitmap.buffer[i * bitmap.pitch + 3 * j + 2] / 255.;
			size_t idx = 3 * (left + j) + dest->pitch * (top + i);
			dest->data[idx + 0] = R;
			dest->data[idx + 1] = G;
			dest->data[idx + 2] = B;
		}
	}
}

static inline void copy2(GlyphSlot *dest, FT_Bitmap bitmap, long top, long left) {
	for (int i = 0; i < bitmap.rows; ++i) {
		for (int j = 0; j < bitmap.width; ++j) {
			float val = bitmap.buffer[i * bitmap.pitch + j] / 255.;
			size_t idx = 3 * (left + j) + dest->pitch * (top + i);
			dest->data[idx + 0] = val;
			dest->data[idx + 1] = val;
			dest->data[idx + 2] = val;
		}
	}
}

static inline void copy1(GlyphSlot *dest, FT_Bitmap bitmap, long top, long left) {
	for (int i = 0; i < bitmap.rows; ++i) {
		for (int j = 0; j < bitmap.width; ++j) {
			char pixel = bitmap.buffer[i * bitmap.pitch + j / 8]; // Is this correct?
			if (pixel & (0x80 >> j)) {
				size_t idx = 3 * (left + j) + dest->pitch * (top + i);
				dest->data[idx + 0] = 1.;
				dest->data[idx + 1] = 1.;
				dest->data[idx + 2] = 1.;
			}
		}
	}
}

// buf format is float RGB
void render_glyph(GlyphSlot *slot, hb_buffer_t *hb_buffer, size_t start, size_t end, hb_font_t *hb_font, double scale) {
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, NULL);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);

	FT_Face face = hb_ft_font_get_face(hb_font);

	long ascent = ceil(face->size->metrics.ascender * scale / 64.);
	long descent = ceil(-face->size->metrics.descender * scale / 64.);
	long glyph_height = ascent + descent;

	for (int i = start; i < end; ++i) {
		int gid = info[i].codepoint;
		int x_offset =  round((double) pos[i].x_offset * scale / 64.), y_offset = round((double) pos[i].y_offset * scale / 64.);

		FT_ASSERT(FT_Load_Glyph(face, gid, FT_LOAD_COLOR | FT_LOAD_DEFAULT));
		FT_ASSERT(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)); // FT_RENDER_MODE_NORMAL)); // FT_RENDER_MODE_LCD));

		long top = slot->height - descent - face->glyph->bitmap_top * scale - y_offset;
		if (top < 0) top = 0; // for now, considering  either clipping or making the cells overlap
		long left = x_offset + face->glyph->bitmap_left * scale;

		FT_Bitmap bitmap = face->glyph->bitmap;
		// TODO(ym): idk implement this better somehow
		switch (bitmap.pixel_mode) {
			case 7:
				area_averaging_scale(slot, bitmap, top, left, scale);
				break;
			case 5:
				copy5(slot, bitmap, top, left);
				break;
			case 2:
				copy2(slot, bitmap, top, left);
				break;
			case 1:
				copy1(slot, bitmap, top, left);
				break;
			default:
				die("Pixel mode %d has not been implemented yet, please contact the developer\n", bitmap.pixel_mode);
				break;
		}
	}
}

void render_segment(uint32_t *str, size_t len, size_t offset, size_t seqlen, hb_script_t script, FriBidiLevel level, bool is_emoji) {
	static hb_buffer_t *hb_buffer;
	static hb_unicode_funcs_t *unicodedefs;
	if (!unicodedefs) {
		unicodedefs = hb_unicode_funcs_get_default();
	}
	if (unlikely(!hb_buffer)) {
		hb_buffer = hb_buffer_create();
		hb_buffer_pre_allocate(hb_buffer, 4096);
		assert(hb_buffer_allocation_successful(hb_buffer));
		hb_buffer_set_unicode_funcs(hb_buffer, unicodedefs);
		hb_buffer_set_flags(hb_buffer, HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES);
	}

	hb_buffer_clear_contents(hb_buffer);
	hb_buffer_add_codepoints(hb_buffer, str, len, offset, seqlen);
	hb_buffer_set_script(hb_buffer, script);
	hb_buffer_set_direction(hb_buffer, level % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
	hb_buffer_guess_segment_properties(hb_buffer);

	FT_Face fc;
	double pixelfixup = find_font(ft_library, &fc, is_emoji ? "und-zsye" : "", str + offset, seqlen);

	hb_font_t *hb_font = hb_ft_font_create (fc, NULL);
	hb_shape (hb_font, hb_buffer, NULL, 0);
	print_buffer(hb_buffer, hb_font);

	/* draw(&dc, hb_buffer, pixelfixup, hb_font); */
	hb_font_destroy (hb_font);
}

FriBidiLevel *get_embedding_levels(uint32_t *str, size_t len, FriBidiParType *dir) {
	FriBidiCharType *bidi_types = malloc(len * sizeof(*bidi_types));
	fribidi_get_bidi_types(str, len, bidi_types);
	FriBidiBracketType *bracket_types = malloc(len * sizeof(*bracket_types));
	fribidi_get_bracket_types(str, len, bidi_types, bracket_types);

	FriBidiLevel *embedding_levels = malloc(len * sizeof(*embedding_levels));
	fribidi_get_par_embedding_levels_ex(bidi_types, bracket_types, len, dir, embedding_levels);

	free(bidi_types);
	free(bracket_types);

	return embedding_levels;
}

void shape(uint32_t *str, size_t len, FriBidiParType *dir) {
	FriBidiLevel *embedding_levels = get_embedding_levels(str, len, dir);
	print_bidi(embedding_levels, len);

	// TODO(ym): This goes ever each glyph multiple times per itemizer; fix that
	// TODO(ym): Probably should be like: embedding -> script -> emoji, rather than script -> embedding -> emoji
	size_t current = 0;
	while (current < len) {
		hb_script_t script;
		uint32_t *end = itemize_script(str + current, len - current, &script);

		FriBidiLevel bidilevel;
		FriBidiLevel *end2 = itemize_embeddings(embedding_levels + current, end - str - current, &bidilevel);

		bool is_emoji;
		end = itemize_emoji(str + current, end2 - embedding_levels - current, &is_emoji);

		// Shouldn't be done here
		render_segment(str, len, current, end - str - current, script, bidilevel, is_emoji);
		current = end - str;
	}
}

int main(int argc, char *argv[]) {
	FT_Init_FreeType (&ft_library);
	FT_Library_SetLcdFilter(ft_library, FT_LCD_FILTER_DEFAULT);

	char str[] = "Hello world\xF0\x9F\x98\x8A\xF0\x9F\x98\xB6\xE2\x80\x8D\xf0\x9f\x8c\xab\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7\xe2\x80\x8d\xf0\x9f\x91\xa7\xf0\x9f\xa4\xb9\xf0\x9f\x8f\xbe\xe2\x80\x8d\xe2\x99\x80\xef\xb8\x8f\xf0\x9f\x91\xa8\xf0\x9f\x8f\xbc\xe2\x80\x8d\xf0\x9f\x9a\x80 السلام عليكم";
	size_t len = strlen(str);

	uint32_t *buf = malloc(len * sizeof(*buf));
	FriBidiParType dir = 0;

	size_t utf32len = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, str, len, buf);
	printf("utf32len: %d\n", utf32len);
	shape(buf, utf32len, &dir);
	return 0;
}
