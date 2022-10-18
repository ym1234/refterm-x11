// gcc shape.c -g -fsanitize=undefined -fsanitize=leak -fsanitize=address -lfribidi -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/fribidi

#define ARRAYLEN(arr) (sizeof(arr)/sizeof(*arr))
#include <sys/mman.h>
#include <unistd.h>
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

#include "refterm_glyph_cache.h"
#include "refterm_glyph_cache.c"
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

// Do I need to keep like a fontconfig pattern for this?
// ^ Yes, you do, objects returned by the FcPatternGet functions can't be freed, because they point inside the pattern
// I can always just copy the values, but idk
// Also, the patterns have charsets which are convenient when you are checking whether a font contains a certain glyph or not
// I'm not sure whether we should be checking if the font contains all the glyphs at once or one by one
// and then substituting with another font for the glyphs that actually fail the test
typedef struct {
	FT_Face f;

	FcPattern *pattern;
	unsigned char *path;

	double pxsize;
	double ptsize;
	double scale;

	long width; // 'M' characters width
	long height; // font.ascent + font.descent

	long ascent;
	long descent;

	int load_flags;
	int hintstyle;
} Font;

#define FONT_BUFFER_LENGTH 512
/* // TODO(YM): Maybe we should switch to this model instead */
/* typedef struct { */
/* 	size_t len; */
/* 	FriBidiParType base_dir; */
/* 	uint32_t str[FONT_BUFFER_LENGTH]; */
/* 	FriBidiCharType bidi_types[FONT_BUFFER_LENGTH]; // I don't really need this except to temporary pass it to fribid_get_par_embedding_levels_ex but idk how to remove it */
/* 	FriBidiBracketType bracket_types[FONT_BUFFER_LENGTH]; // Same as above */
/* 	FriBidiLevel embedding_levels[FONT_BUFFER_LENGTH]; */
/* 	uint64_t idx[FONT_BUFFER_LENGTH]; // TODO(YM): idk */

/* 	hb_buffer_t *hb_buffer; */
/* } ShapingContext; */

typedef struct {
	FT_Library ft_library;
	Font main_font;
	glyph_table *CacheTable;

#define MinDirectCodepoint 32
#define MaxDirectCodepoint 126
	gpu_glyph_index ReservedTileTable[MaxDirectCodepoint - MinDirectCodepoint + 1];


	size_t len;
	FriBidiParType base_dir;
	uint32_t str[FONT_BUFFER_LENGTH];
	FriBidiCharType bidi_types[FONT_BUFFER_LENGTH]; // I don't really need this except to temporary pass it to fribid_get_par_embedding_levels_ex but idk how to remove it
	FriBidiBracketType bracket_types[FONT_BUFFER_LENGTH]; // Same as above
	FriBidiLevel embedding_levels[FONT_BUFFER_LENGTH];

	// high dword is idx, low is len
	uint64_t idx[FONT_BUFFER_LENGTH]; // TODO(YM): idk

	hb_buffer_t *hb_buffer;
	float *render_buffer;
	unsigned char *render_buffer2point0; // It's 10x better with 10x less space!?!??!?!
	size_t render_glyph_num;
	size_t render_buffer_size;
	size_t glyph_count;
	/* GlyphSlot slot; // TODO(ym): support batch uploads, instead of the render-to-the-slot-then-upload approach */
} FontSystem;

long getps(void) {
#if 0
	return getpagesize();
#else
	return sysconf(_SC_PAGESIZE);
#endif
}

long pagealigned(long x) {
	// Not a "universal" align primitive (i think?), but works well enough
#define ALIGN(x, y) (((x) + (y) - 1) & ~((y) - 1))
    long pagesize = getps();
    return ALIGN(x, pagesize);
#undef ALIGN
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
			size_t idx = 4 * (left + j) + dest->pitch * (top + i);
			dest->data[idx + 0] = dest->data[idx + 0] + R;
			dest->data[idx + 1] = dest->data[idx + 1] + G;
			dest->data[idx + 2] = dest->data[idx + 2] + B;
			dest->data[idx + 3] = (	dest->data[idx + 0] +	dest->data[idx + 1] +	dest->data[idx + 2] )/3; // idk
		}
	}
}

static inline void copy2(GlyphSlot *dest, FT_Bitmap bitmap, long top, long left) {
	for (int i = 0; i < bitmap.rows; ++i) {
		for (int j = 0; j < bitmap.width; ++j) {
			float val = bitmap.buffer[i * bitmap.pitch + j] / 255.;
			size_t idx = 4 * (left + j) + dest->pitch * (top + i);
			dest->data[idx + 0] = val;
			dest->data[idx + 1] = val;
			dest->data[idx + 2] = val;
			dest->data[idx + 3] = val;
		}
	}
}

static inline void copy1(GlyphSlot *dest, FT_Bitmap bitmap, long top, long left) {
	for (int i = 0; i < bitmap.rows; ++i) {
		for (int j = 0; j < bitmap.width; ++j) {
			char pixel = bitmap.buffer[i * bitmap.pitch + j / 8]; // Is this correct?
			if (pixel & (0x80 >> j)) {
				size_t idx = 4 * (left + j) + dest->pitch * (top + i);
				dest->data[idx + 0] = 1.;
				dest->data[idx + 1] = 1.;
				dest->data[idx + 2] = 1.;
				dest->data[idx + 3] = 1.;
			}
		}
	}
}


void render_glyph(GlyphSlot *slot, Font face, int gid, int x_offset, int y_offset) {
	FT_ASSERT(FT_Load_Glyph(face.f, gid, FT_LOAD_COLOR | FT_LOAD_DEFAULT)) // TODO(YM): use the face.load_flags;
	FT_ASSERT(FT_Render_Glyph(face.f->glyph, FT_RENDER_MODE_LCD));
	long top = slot->height - face.descent - face.f->glyph->bitmap_top * face.scale - y_offset;
	if (top < 0) top = 0;
	long left = x_offset + face.f->glyph->bitmap_left * face.scale;

	FT_Bitmap bitmap = face.f->glyph->bitmap;
	printf("pixel_mode: %d\n", bitmap.pixel_mode);
	switch (bitmap.pixel_mode) {
		case 7:
			area_averaging_scale(slot, bitmap, top, left, face.scale);
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
		case 0:
			printf("Freetype couldn't render the glyph, probably was .nodef\n");
			break;
		default:
			die("Pixel mode %d has not been implemented yet, please contact the developer\n", bitmap.pixel_mode);
			break;
	}
}
#define FONT_SIZE 12
Font face_from_pattern(FT_Library ft_library, FcPattern *m, size_t requested_size) {
	Font face = {};

	// TODO(YM): Apparently FcPatternGet* can fail and they return FcResult?
	// Ugh Handle that
	face.pattern = m;
	FcPatternGetString(m, FC_FILE, 0, &face.path);
	face.scale = 1;
	FcPatternGetDouble(m, "pixelsizefixupfactor", 0, &face.scale);

	if (FT_New_Face(ft_library, face.path, 0, &face.f)) {
		die("Couldn't create freetype face for font: %s", face.path);
	}

	FcPatternGetDouble(m, FC_PIXEL_SIZE, 0, &face.pxsize);
	FcPatternGetDouble(m, FC_SIZE, 0, &face.ptsize);


	printf("pxsize: %f\n", face.pxsize);
	if (FT_Set_Pixel_Sizes(face.f, 0, ceil(face.pxsize))) {
		int result = 0;
		for (int i = 0; i < face.f->num_fixed_sizes; ++i) {
			FT_Bitmap_Size size = face.f->available_sizes[i];
			// TODO(YM):
			// if (size.height * face.scale < requested_size && /* Check if it is bigger than the last size */) result = i;
			printf("height: %d, width: %d\n", size.height, size.width);
		}

		FT_Bitmap_Size size = face.f->available_sizes[0];
		// TODO(YM): I don't know if this sets the metrics or not, so just set them manually for now
		FT_Select_Size(face.f, 0);
		face.height = size.height;
		face.ascent = size.height;
		face.descent = 0;
	} else {
		face.ascent = ceil(face.f->size->metrics.ascender * face.scale / 64.);
		face.descent = ceil(-face.f->size->metrics.descender * face.scale / 64.);
		face.height = face.ascent + face.descent;
		printf("ascent: %d, descent: %d, scale: %f\n", face.ascent, face.descent, face.scale);
	}

	FT_ASSERT(FT_Load_Char(face.f, 'M', FT_LOAD_DEFAULT));
	face.width = ceil(face.f->glyph->advance.x * face.scale / 64.);

	printf("advance.x: %d\n", face.f->glyph->advance.x);
	printf("width: %d\n", face.width);

	FcMatrix *fc_matrix;
	if (FcPatternGetMatrix(m, FC_MATRIX, 0, &fc_matrix) == FcResultMatch) {
		FT_Matrix tm = {
			.xx = fc_matrix->xx * 0x10000,
			.xy = fc_matrix->xy * 0x10000,
			.yx = fc_matrix->yx * 0x10000,
			.yy = fc_matrix->yy * 0x10000,
		};
		FT_Set_Transform(face.f, &tm, NULL); // Doesn't seem to do anything?
	}
	return face;
}

Font find_font(FT_Library ft_library, char *language, unsigned int *utf32, int len) {
	FcInit();
	FcPattern *p = FcPatternCreate(); // TODO(ym): Destroy this later
	FcCharSet *s = FcCharSetCreate();
	FcLangSet *l = FcLangSetCreate();

	FcLangSetAdd(l, language);
	for (int k = 0; k < len; k++) {
		FcCharSetAddChar(s, utf32[k]);
	}

	/* FcPatternAddString(p, FC_STYLE, "monospace"); */
	/* FcPatternAddString(p, FC_SPACING, "monospace"); */
	FcPatternAddDouble(p, FC_PIXEL_SIZE, FONT_SIZE);
	/* FcPatternAddDouble(p, FC_SIZE, FONT_SIZE); */
	FcPatternAddCharSet(p, FC_CHARSET, s);

	FcConfigSubstitute(NULL, p, FcMatchPattern);
	FcDefaultSubstitute(p);

	FcResult result;
	FcPattern *m = FcFontMatch(NULL, p, &result);
	if (result != FcResultMatch) return (Font) {};
	return face_from_pattern(ft_library, m, FONT_SIZE);
}

// Probably will change in the future
void init_glyphslot(GlyphSlot *slot, size_t glyph_width, size_t glyph_height) {
	slot->width = glyph_width, slot->height = glyph_height;
	slot->data = calloc(slot->width * slot->height * 4, sizeof(float));
}

FontSystem *fontsystem_new(FT_Library ft_library, Font main_font, size_t texture_width, size_t texture_height) {
	size_t size = pagealigned(sizeof(FontSystem));

	FontSystem *system = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	printf("%zu\n", size);
	assert(system != MAP_FAILED);

	system->ft_library = ft_library;

	// Combine these into one mmap call
	system->main_font = main_font;

	/* glyph_table_params params = {}; */
	/* params.ReservedTileCount = ARRAYLEN(system->ReservedTileTable); */
	/* params.CacheTileCountInX = (texture_width / main_font.width); */
	/* params.EntryCount = (texture_width / main_font.width) * (texture_height / (main_font.ascent + main_font.descent)); */
	/* if (params.EntryCount > params.ReservedTileCount) params.EntryCount - params.ReservedTileCount; */
	/* params.HashCount = 8192; */

	/* size_t cachetablesize = pagealigned(GetGlyphTableFootprint(params)); */

	/* system->CacheTable = mmap(NULL, cachetablesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); */
	/* assert(system->CacheTable != MAP_FAILED); */
	/* system->CacheTable = PlaceGlyphTableInMemory(params, system->CacheTable); */
	system->CacheTable = NULL; // TODO(ym): just for testing

	system->render_glyph_num = 50; // TODO(YM): Set this to a reasonable number
	system->render_buffer_size = pagealigned(4 * system->render_glyph_num  * (main_font.ascent + main_font.descent) * main_font.width);
	printf("buffer_size: %zu, font width: %zu, font height: %zu, glyph_num: %zu\n", system->render_buffer_size, main_font.width, main_font.ascent + main_font.descent, system->render_glyph_num);
	system->render_buffer = mmap(NULL, system->render_buffer_size * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	system->render_buffer2point0 = mmap(NULL, system->render_buffer_size * sizeof(char), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int leftover = (system->render_buffer_size - (4 * system->render_glyph_num  * (main_font.ascent + main_font.descent) * main_font.width)) / (main_font.height * main_font.width);
	/* system->render_glyph_num += leftover; // TODO(ym): this breaks shit, investigate later */
	printf("leftover: %d\n", leftover);
	/* render_buffer = */
	/* init_glyphslot(&system->slot, main_font.width, main_font.ascent + main_font.descent); */

	system->hb_buffer = hb_buffer_create();
	hb_buffer_pre_allocate(system->hb_buffer, FONT_BUFFER_LENGTH);
	assert(hb_buffer_allocation_successful(system->hb_buffer));
	hb_buffer_set_unicode_funcs(system->hb_buffer, hb_unicode_funcs_get_default());
	return system;
}


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

// Maybe I should pass everything that should be allocated explicitly and not just pass FontSystem?
void get_embedding_levels(FontSystem *system) {
	fribidi_get_bidi_types(system->str, system->len, system->bidi_types);
	fribidi_get_bracket_types(system->str, system->len, system->bidi_types, system->bracket_types);
	fribidi_get_par_embedding_levels_ex(system->bidi_types, system->bracket_types, system->len, &system->base_dir, system->embedding_levels);
}

// TODO(YM): free shit, god why is this library so shitty
bool intersect(FcPattern *p, uint32_t *str, size_t len) {
	FcCharSet *charset;
	FcPatternGetCharSet(p, FC_CHARSET, 0, &charset);

	FcCharSet *s = FcCharSetCreate();
	for (int k = 0; k < len; k++) {
		FcCharSetAddChar(s, str[k]);
	}
	return FcCharSetIntersectCount(charset, s) == FcCharSetCount(s);
}

void render_segment(FontSystem *system, size_t offset, size_t seqlen, hb_script_t script, FriBidiLevel level, bool is_emoji) {
	hb_buffer_clear_contents(system->hb_buffer);
	hb_buffer_add_codepoints(system->hb_buffer, system->str, system->len, offset, seqlen);
	hb_buffer_set_script(system->hb_buffer, script);
	hb_buffer_set_direction(system->hb_buffer, level % 2 == 0 ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
	hb_buffer_guess_segment_properties(system->hb_buffer);


	Font font = {};
	if (intersect(system->main_font.pattern, system->str + offset, seqlen)) {
		font = system->main_font;
	} else {
		font = find_font(system->ft_library, is_emoji ? "und-zsye" : "", system->str + offset, seqlen);
	}

	hb_font_t *hb_font = hb_ft_font_create (font.f, NULL);
	hb_shape (hb_font, system->hb_buffer, NULL, 0);
	print_buffer(system->hb_buffer, hb_font);


	size_t len = hb_buffer_get_length(system->hb_buffer);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (system->hb_buffer, NULL);
	hb_glyph_info_t *info = hb_buffer_get_glyph_infos (system->hb_buffer, NULL);

	long current_cluster = -1;
	/* int glyph_counter = 0; */
	GlyphSlot slot = {};
	for (int i = 0; i < len; ++i) {
		hb_codepoint_t gid  = info[i].codepoint;
		long cluster = (long) info[i].cluster;
		int x_offset =  round((double) pos[i].x_offset * system->main_font.scale / 64.), y_offset = round((double) pos[i].y_offset * system->main_font.scale / 64.);
		if (current_cluster != cluster) {
			if (system->glyph_count >= system->render_glyph_num) {
				break;
			}
			current_cluster = cluster;
			slot.width = system->main_font.width;
			slot.height = system->main_font.height;
			slot.pitch = system->main_font.width * system->render_glyph_num * 4;
			slot.data = system->render_buffer + system->glyph_count * system->main_font.width * 4;
			system->glyph_count++;
		}
		render_glyph(&slot, font, gid, x_offset, y_offset);
	}
	/* draw(&dc, system->hb_buffer, pixelfixup, hb_font); */
	hb_font_destroy (hb_font);
}


size_t fontsystem_shape(FontSystem *system, char* utf8, ssize_t len) { // Mixing unsigned and signed values like this feels really dirty
	ssize_t leftover =  len - FONT_BUFFER_LENGTH;

	// This is really ugly lol
	len = len >= FONT_BUFFER_LENGTH ? FONT_BUFFER_LENGTH : len;
	len = system->len = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, utf8, len, system->str);
	/* len = system->len; */

	get_embedding_levels(system);

	size_t current = 0;
	while (current < len) {
		FriBidiLevel bidilevel;
		FriBidiLevel *end2 = itemize_embeddings(system->embedding_levels + current, len - current, &bidilevel);

		hb_script_t script;
		uint32_t *end = itemize_script(system->str + current, end2 - system->embedding_levels - current, &script);

		bool is_emoji;
		end = itemize_emoji(system->str + current, end - system->str - current, &is_emoji);

		// Shouldn't be done here
		/* render_segment(system->str, len, current, end - system->str - current, script, bidilevel, is_emoji); */
		render_segment(system, current, end - system->str - current, script, bidilevel, is_emoji);
		current = end - system->str;
	}

	return leftover < 0 ? 0 : leftover;
}

void float2char(float *in, unsigned char *out, int height, int width, int pitch) {
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			int idx = 4 * (i * pitch + j);
			int R = in[idx + 0] * 255;
			int B = in[idx + 1] * 255;
			int G = in[idx + 2] * 255;
			int A = in[idx + 3] * 255;
			if (R > 255) R = 255;
			if (B > 255) B = 255;
			if (G > 255) G = 255;
			if (A > 255) A = 255;
			out[idx + 0] = R;
			out[idx + 1] = G;
			out[idx + 2] = B;
			out[idx + 3] = A;
		}
	}
}

int main(int argc, char *argv[]) {
	FcInit();

	FT_Library ft_library;

	FT_ASSERT(FT_Init_FreeType (&ft_library));
	/* FT_ASSERT(FT_Library_SetLcdFilter(ft_library, FT_LCD_FILTER_DEFAULT)); */

	unsigned char text[100] = {};
	unsigned char *patternstring = snprintf(text, 100, "curie:pixelsize=%d", FONT_SIZE);
	FcPattern *mfpattern = FcNameParse(text);
	FcConfigSubstitute(NULL, mfpattern, FcMatchPattern);
	FcDefaultSubstitute(mfpattern);

	FcResult result;
	FcPattern *m = FcFontMatch(NULL, mfpattern, &result);
	if (result != FcResultMatch) die("Couldn't load font from pattern string: %s", patternstring);
	Font main_font =  face_from_pattern(ft_library, m, FONT_SIZE);

	FontSystem *system = fontsystem_new(ft_library, main_font, 0, 0);

	/* char str[] = "Hello world"; */
	/* char str[] = "Hello world 日本語"; */

	/* char str[] = "قَدْ سَرَى فِي جَسَدِي.."; */
	/* char *str ="ꜱ͘ɪʙ̢ᴇ҉ʟ͞ɪ͟ᴜ͡ꜱ̶ ̸ᴄʀ̕ᴀ̢ꜱ҉ʜ̶ᴇ͞ᴅ!͏"; */
	char str[] = "Hello world! 日本語";
	/* char str[] = "Hello world السلام عليكم 日本語"; */
	size_t len = strlen(str);
	printf("len: %d\n", len);

	fontsystem_shape(system, str, len);

	/* float *x = system->render_buffer; */
	/* unsigned char *x = system->render_buffer2point0; */
	float2char(system->render_buffer, system->render_buffer2point0, system->main_font.ascent + system->main_font.descent, system->main_font.width * system->render_glyph_num, system->main_font.width * system->render_glyph_num);
	stbi_write_png("idk.png", system->render_glyph_num * system->main_font.width, system->main_font.ascent + system->main_font.descent, 4, system->render_buffer2point0, system->render_glyph_num * system->main_font.width * 4);
	/* stbi_write_png("idk.png", dc.width, dc.height, 4, dc.buf, dc.width * 4); */
	return 0;
}
