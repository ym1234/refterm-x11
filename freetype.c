#define FT_CONFIG_OPTION_ERROR_STRINGS
#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct {
    FT_Face face;

    size_t descent;
    size_t ascent;

    size_t cellwidth;
    size_t cellheight;

    size_t underline;
    size_t underline_thickness;
} FTFont;

#define FT_CHECK(x) \
    do {\
		FT_Error error;\
        if (error = (x)) {\
            die("%s:%d 0x%x, %s\n", __FILE__, __LINE__, error, FT_Error_String(error));\
        }\
    } while (0)

#define INDEX(k, f, l, m) (4 * ((k) * columns * rows * 10 + (f) * columns + (l) * columns * 10 + (m)))

static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";


static inline float *render_cursor(float *data, int rows, int columns) {
    size_t charnum = strlen(ascii_printable);
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

static inline float *render_baseline(float *data, int rows, int columns, int descent) {
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
    int charnum = strlen(ascii_printable);

    FT_Bitmap bitmap;
    FT_Bitmap_Init(&bitmap);

    for (int i = 0; i < charnum; i++) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, ascii_printable[i]);
        FT_CHECK(FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER));
        FT_CHECK(FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_NORMAL));
        FT_CHECK(FT_Bitmap_Convert(library, &face->glyph->bitmap, &bitmap, 1));

        int left = face->glyph->bitmap_left;
        int top = rows - descent - face->glyph->bitmap_top;
        if (top < 0) {
            printf("top < 0, clipping\n");
            top = 0;
        }

		/* if (bitmap.pitch < 0) { // is this needed? or actually is this correct?? */
		/*     printf("pitch less than 0: %d\n", bitmap.pitch); */
		/*     bitmap.pitch =  -bitmap.pitch; */
		/* } */

        int k = i / 10;
        int f = i % 10;
        for (int l = 0; l < bitmap.rows; l++) {
            for (int m = 0; m < bitmap.width; m++) {
                int index = INDEX(k, f, l + top, m + left);
                if (index < 0 || index >= (rows * 10) * (columns * 10) * 4) {
                    printf("Failed to blit %c\n", ascii_printable[i]);
                    goto x;
                }
                float pixel = (float) bitmap.buffer[l * bitmap.pitch + m];
                float num_grays = (float) bitmap.num_grays - 1;
                if (pixel) {
                    float z = pixel / num_grays;
                    data[index] = z;
                    data[index + 1] = z;
                    data[index + 2] = z;
                    data[index + 3] = z;
                }
            }
        }
x:
    }
    FT_Bitmap_Done(library, &bitmap);

    render_cursor(data, rows, columns);
    /* render_baseline(data, rows, columns, descent); */
    return data;
}

FTFont load_font(FT_Library library, char *fc, int width, int height) {
    FT_Face face;
    FT_CHECK(FT_New_Face(library, fc, 0, &face));
    if (FT_IS_SCALABLE(face)) {
        FT_CHECK(FT_Set_Pixel_Sizes(face, 0, height));
    } else  {
        FT_Select_Size(face, 0); // TODO
    }
    FT_CHECK(FT_Load_Char(face, 'M', FT_RENDER_MODE_NORMAL));

    FTFont font = {
        .face = face,
        .ascent = face->size->metrics.ascender >> 6,
        .descent = -face->size->metrics.descender >> 6,

        .cellheight = font.ascent + font.descent,
        .cellwidth  = face->glyph->advance.x >> 6,

        .underline = -face->underline_position >> 6,
        .underline_thickness = face->underline_thickness >> 6
    };

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
