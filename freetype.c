#define FT_CONFIG_OPTION_ERROR_STRINGS
#define FT_CONFIG_OPTION_SUBPIXEL_RENDERING
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_LCD_FILTER_H

typedef struct {
	FT_Face face;

	int descent;
	int ascent;

	int cellwidth;
	int cellheight;

	int underline;
	int underline_thickness;
} FTFont;

#define FT_ASSERT(x) \
	do {\
		FT_Error error;\
		if (error = (x)) {\
			die("%s:%d 0x%x, %s\n", __FILE__, __LINE__, error, FT_Error_String(error));\
		}\
	} while (0);

#define INDEX(k, f, l, m) (4 * ((k) * columns * rows * 10 + (f) * columns + (l) * columns * 10 + (m)))

#define ARRSIZE(x) (sizeof(x) / sizeof(*x))
static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";


static inline void render_cursor(float *data, int rows, int columns) {
	size_t charnum = ARRSIZE(ascii_printable) - 1;
	int k = charnum / 10;
	int f = charnum % 10;
	for (int i = 0; i < rows; i++) {
		int index = INDEX(k, f, i, 0);
		for (int j = 0; j < 4 * columns; j += 4) {
			float z = 1;
			data[index + j] = z;
			data[index + j + 1] = z;
			data[index + j + 2] = z;
			data[index + j + 3] = z;
		}
	}
}

static inline void render_baseline(float *data, int rows, int columns, int descent) {
	for (int i = 0; i < 10; i++) {
		int i1 = INDEX(i + 1, 0, -1, 0);
		int i2 = INDEX(i, 0, rows - descent, 0);
		for (int k = 0; k < columns * 10 * 4; k += 4) { // could use a fore loop here
			// height
			data[i1 + k]     = 1;
			data[i1 + k + 1] = 1;
			data[i1 + k + 2] = 1;
			data[i1 + k + 3] = 1;
			// baseline
			data[i2 + k]     = 1;
			data[i2 + k + 1] = 1;
			data[i2 + k + 2] = 1;
			data[i2 + k + 3] = 1;
		}
	}
}

float *render_ascii(FTFont font, FT_Library library) {
	int rows = font.cellheight, columns = font.cellwidth, descent = font.descent;
	FT_Face face = font.face;

	float *data = calloc((rows * 10) * (columns * 10) * 4, sizeof(float));
	int charnum = ARRSIZE(ascii_printable) - 1;

	/* FT_Bitmap bitmap; */
	/* FT_Bitmap_Init(&bitmap); */

	for (int i = 0; i < charnum; i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(face, ascii_printable[i]);
		/* FT_ASSERT(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT));// FT_LOAD_RENDER)); */
		/* FT_ASSERT(FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_NORMAL | FT_LOAD_TARGET_LIGHT)); */
		/* printf("Character: %c, Bitmap: rows: %d, width: %d, pitch: %d, pixel_mode: %d, x_advance: %d\n", ascii_printable[i], face->glyph->bitmap.rows, face->glyph->bitmap.width, face->glyph->bitmap.pitch, face->glyph->bitmap.pixel_mode, face->glyph->advance.x); */
		/* /1* FT_ASSERT(FT_Bitmap_Convert(library, &face->glyph->bitmap, &bitmap, 1)); *1/ */
		FT_ASSERT(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT));// FT_LOAD_RENDER));
		FT_ASSERT(FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_LCD));
		/* printf("Character: %c, Bitmap: rows: %d, width: %d, pitch: %d, pixel_mode: %d x_advance: %d\n", ascii_printable[i], face->glyph->bitmap.rows, face->glyph->bitmap.width, face->glyph->bitmap.pitch, face->glyph->bitmap.pixel_mode, face->glyph->advance.x); */

		FT_Bitmap bitmap = face->glyph->bitmap;

		int left = face->glyph->bitmap_left;
		int top = (rows - descent - face->glyph->bitmap_top);
		if (top < 0) {
			printf("top < 0, clipping\n");
			top = 0;
		}

		// TODO(ym): Just use FT_Bitmap_Convert
		int k = i / 10;
		int f = i % 10;
		for (int l = 0; l < bitmap.rows; ++l) {
			int index = INDEX(k, f, l + top, 0);
			index += left * 4;
			for (int z = 0; z < bitmap.width; ++z) {
				switch (bitmap.pixel_mode) {
					case 5:
						if ((index % 4) == 3) {
							data[index] = 1;
							++index;
							break;
						}
						data[index] = bitmap.buffer[l * bitmap.pitch + z] / 255.;
						++index;
						break;
					case 1:
						char pixel = bitmap.buffer[l * bitmap.pitch];
						if (pixel & (0x80 >> z)) {
							data[index + 4 * z] = 1;
							data[index + 4 * z + 1] = 1;
							data[index + 4 * z + 2] = 1;
							data[index + 4 * z + 3] = 1;
						}
						break;
				}
			}
		}
	}
	/* FT_Bitmap_Done(library, &bitmap); */

	render_cursor(data, rows, columns);
	/* render_baseline(data, rows, columns, descent); */
	return data;
}

FTFont load_font(FT_Library library, char *fc, int width, int height) {
	FT_Face face;
	FT_ASSERT(FT_New_Face(library, fc, 0, &face));
	float factor = 1.0; // TODO(ym): set this according to subpixel antialiasing
	if (FT_IS_SCALABLE(face)) {
		factor = 1.15;
		FT_ASSERT(FT_Set_Char_Size(face, width << 6, height << 6, 0, 0));
		/* FT_ASSERT(FT_Set_Pixel_Sizes(face, 0, height)); */
	} else  {
		FT_Select_Size(face, 0); // TODO
	}
	FT_ASSERT(FT_Load_Char(face, 'M', FT_LOAD_DEFAULT));
	/* FT_ASSERT(FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_LCD)); */

	printf("Xadvance: %d, >>6: %d\n", face->glyph->advance.x, face->glyph->advance.x >> 6);
	FTFont font = {
		.face = face,
		.ascent = face->size->metrics.ascender >> 6,
		.descent = -face->size->metrics.descender >> 6,

		.cellheight = font.ascent + font.descent,
		.cellwidth  = (face->glyph->advance.x >> 6) / factor, // factor: useful for subpixel antialiased fonts because they report their width a little too big for some reason?

		.underline = -face->underline_position >> 6,
		.underline_thickness = face->underline_thickness >> 6
	};
	/* printf("max_advance: %d", face->size->metrics.max_advance >> 6); */
	/* font.cellwidth  = face->size->metrics.max_advance >> 6; */

	if (!font.underline_thickness) {
		font.underline = 1;
		font.underline_thickness = 1;
	}
	/* font.underline = 5; */

	printf("height: %d, width: %d\n", font.cellheight, font.cellwidth);
	printf("ascent: %d, descent: %d\n", font.ascent, font.descent);
	printf("underline start: %d, underline thickness: %d\n", font.underline, font.underline_thickness);
	printf("raw underline start: %d, raw underline thickness: %d\n", -face->underline_position, face->underline_thickness);
	return font;
}
