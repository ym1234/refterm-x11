// build: gcc shape.c -fsanitize=undefined -fsanitize=leak -fsanitize=address -lm -lcairo -lfontconfig -lharfbuzz  -lfreetype -I/usr/include/freetype2 -I/usr/include/fontconfig/ -I/usr/include/harfbuzz/ -I/usr/include/cairo/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fontconfig/fontconfig.h>
#include <hb.h>
#include <hb-ft.h>
#include <assert.h>
#include <cairo.h>
#include <cairo-ft.h>

typedef struct {
	int start;
	int length;
	FT_Face ft_face;
	int advance_x;
	int advance_y;
} Range;

#define FONT_SIZE 36
#define MARGIN (FONT_SIZE * .5)

// Taken from st
#define LEN(a)			(sizeof(a) / sizeof(a[0]))
#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))

#define UTF_INVALID   0xFFFD
#define UTF_SIZ       4
#define uchar unsigned char
#define Rune unsigned int

static const uchar utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const uchar utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const Rune utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const Rune utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

Rune utf8decodebyte(char c, size_t *i) {
	for (*i = 0; *i < LEN(utfmask); ++(*i))
		if (((uchar)c & utfmask[*i]) == utfbyte[*i])
			return (uchar)c & ~utfmask[*i];

	return 0;
}

size_t utf8decode(const char *c, Rune *u, size_t clen) {
	size_t i, j, len, type;
	Rune udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type != 0)
			return j;
	}
	if (j < len)
		return 0;
	*u = udecoded;
	/* utf8validate(u, len); */

	return len;
}

int shape(FT_Face ft_face, hb_buffer_t *hb_buffer) {
	/* Create hb-ft font. */
	hb_font_t *hb_font;
	hb_font = hb_ft_font_create (ft_face, NULL);


	/* Shape it! */
	hb_shape (hb_font, hb_buffer, NULL, 0);

	/*   /1* Get glyph information and positions out of the buffer. *1/ */
	  unsigned int len = hb_buffer_get_length (hb_buffer);
	  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
	  hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (hb_buffer, NULL);

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

	    printf ("glyph='%s'	cluster=%d	advance=(%g,%g)	offset=(%g,%g)\n",
	            glyphname, cluster, x_advance, y_advance, x_offset, y_offset);
	  }

	  printf ("Converted to absolute positions:\n");
	  /* And converted to absolute positions. */
	  {
	    double current_x = 0;
	    double current_y = 0;
	    for (unsigned int i = 0; i < len; i++)
	    {
	      hb_codepoint_t gid   = info[i].codepoint;
	      unsigned int cluster = info[i].cluster;
	      double x_position = current_x + pos[i].x_offset / 64.;
	      double y_position = current_y + pos[i].y_offset / 64.;


	      char glyphname[32];
	      hb_font_get_glyph_name (hb_font, gid, glyphname, sizeof (glyphname));

	      printf ("glyph='%s'	cluster=%d	position=(%g,%g)\n",
		      glyphname, cluster, x_position, y_position);

	      current_x += pos[i].x_advance / 64.;
	      current_y += pos[i].y_advance / 64.;
	    }
	  }

	hb_font_destroy (hb_font);
}


int draw(Range *range, int r, unsigned int *utf32, int len2, double w, double h, hb_glyph_position_t *pos, int len) {
	double width = 2 * MARGIN + w;
	double height = 2 * MARGIN + h;

	cairo_surface_t *cairo_surface;
	cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil(width), ceil(height));
	cairo_t *cr = cairo_create (cairo_surface);

	cairo_set_source_rgba (cr, 1., 1., 1., 1.);
	cairo_paint (cr);
	cairo_set_source_rgba (cr, 0., 0., 0., 1.);

	/* cairo_translate (cr, MARGIN, MARGIN); */

	double baseline = 0;
	double current_x = 0;
	double current_y = 0;
	for (int i  = 0; i < r; ++i) {
		cairo_font_face_t *cairo_face;
		cairo_face = cairo_ft_font_face_create_for_ft_face (range[i].ft_face, 0);
		cairo_set_font_face (cr, cairo_face);
		cairo_set_font_size (cr, FONT_SIZE);

		if(!baseline) {
			cairo_font_extents_t font_extents;
			cairo_font_extents (cr, &font_extents);
			baseline = (FONT_SIZE - font_extents.height) * .5 + font_extents.ascent;
			cairo_translate (cr, 0, baseline);
		}

		cairo_glyph_t *cairo_glyphs = cairo_glyph_allocate (range[i].length);
		for (int j = 0; j < range[i].length; j++)
		{
			int idx = range[i].start + j;
			cairo_glyphs[j].index = utf32[idx];
			cairo_glyphs[j].x = current_x + pos[idx].x_offset / 64.;
			cairo_glyphs[j].y = -(current_y + pos[idx].y_offset / 64.);
			/* cairo_glyphs[j].y = 0; */

			current_x += pos[idx].x_advance / 64.;
			current_y += pos[idx].y_advance / 64.;
		}
		cairo_show_glyphs (cr, cairo_glyphs, range[i].length);
		cairo_glyph_free (cairo_glyphs);
	}

	cairo_surface_write_to_png (cairo_surface, "out.png");
	cairo_destroy (cr);
	cairo_surface_destroy (cairo_surface);
}


// TODO(ym): cache the fonts
void find_font(FT_Library ft_library, FT_Face *f, unsigned int *utf32, int len) {
	FcPattern *p = FcPatternCreate(); // TODO(ym): Destroy this later
	FcCharSet *s = FcCharSetCreate();
	for (int k = 0; k < len; k++) {
		FcCharSetAddChar(s, utf32[k]);
	}
	FcPatternAddDouble(p, FC_PIXEL_SIZE, FONT_SIZE);
	FcPatternAddCharSet(p, FC_CHARSET, s);
	FcConfigSubstitute(NULL, p, FcMatchPattern);
	FcDefaultSubstitute(p);

	FcResult result;
	FcPattern *m = FcFontMatch(NULL, p, &result);

	unsigned char *name = NULL;
	FcPatternGetString(m, FC_FILE, 0, &name);
	printf("%s\n", name);

	if (FT_New_Face(ft_library, name, 0, f))
		abort();
	if (FT_Set_Char_Size(*f, FONT_SIZE*64, FONT_SIZE*64, 0, 0))
		abort();
}

int main(int argc, char **argv) {
	int i_ = i_;
	if (argc < 2) {
		printf("Please run me with more than 0 arguments");
		return -1;
	}
	const char *text = argv[1];

	if (!FcInit()) {
		fprintf (stderr, "fcinit failed");
		exit(1);
	}

	/* Initialize FreeType and create FreeType font face. */
	FT_Library ft_library;
	FT_Error ft_error;

	if ((ft_error = FT_Init_FreeType (&ft_library)))
		abort();

	/* Create hb-buffer and populate. */
	hb_buffer_t *hb_buffer = hb_buffer_create();
	hb_unicode_funcs_t *def = hb_unicode_funcs_get_default();
	hb_buffer_set_unicode_funcs(hb_buffer, def);

	size_t x = strlen(text);
	unsigned int *utf32 = calloc(x, sizeof(*utf32));

	int i = 0;
	int j = 0;
	while (i < x) {
		unsigned int u = 0;
		i += utf8decode(text + i, &u, x - i);
		utf32[j]  = u;
		++j;
	}

	hb_script_t last = HB_SCRIPT_COMMON;
	hb_glyph_position_t *pos = calloc(j, sizeof(*pos));

	unsigned int *glyphs = calloc(j, sizeof(*glyphs));
	Range *boundries = calloc(j, sizeof(*boundries));

	i = 0;
	int f = 0;

	int lenb = 0;
	double ax = 0;
	double ay = 0;
	int len2 = 0;

	// TODO(ym): clean up

	while (i < j) {
		hb_script_t s = hb_unicode_script(def, utf32[i]);
		if (last != s && __builtin_expect(i != 0, 1) && s != HB_SCRIPT_COMMON && s != HB_SCRIPT_UNKNOWN && s != HB_SCRIPT_INHERITED) {
			FT_Face ft_face;
			find_font(ft_library, &ft_face, utf32 + f, i - f);
			hb_buffer_clear_contents(hb_buffer);
			hb_buffer_add_codepoints(hb_buffer, utf32, j, f, i - f);
			hb_buffer_guess_segment_properties (hb_buffer); // TODO
			shape(ft_face, hb_buffer);

			hb_glyph_position_t *x = hb_buffer_get_glyph_positions (hb_buffer, NULL);
			hb_glyph_info_t *info = hb_buffer_get_glyph_infos (hb_buffer, NULL);
			int z = hb_buffer_get_length(hb_buffer);
			memcpy(pos + len2, x, z * sizeof(*x));
			for (int k = 0; k < z; k++) {
				glyphs[len2 + k] = info[k].codepoint;
				ax += x[k].x_advance / 64.;
				ay += x[k].y_advance / 64.;
			}

			boundries[lenb].start = len2;
			boundries[lenb].length = z;
			boundries[lenb].ft_face = ft_face;
			boundries[lenb].advance_x = ax;
			boundries[lenb].advance_y = ay;
			++lenb;

			len2 += z;
			f = i;
		}
		if (s != HB_SCRIPT_COMMON && s != HB_SCRIPT_UNKNOWN && s != HB_SCRIPT_INHERITED) {
			last = s;
		}
		++i;
	}

	if (i > f) {
		FT_Face ft_face;
		find_font(ft_library, &ft_face, utf32 + f, i - f);

		hb_buffer_clear_contents(hb_buffer);
		hb_buffer_add_codepoints(hb_buffer, utf32, j, f, i - f);
		hb_buffer_guess_segment_properties (hb_buffer);
		shape(ft_face, hb_buffer);

		hb_glyph_position_t *x = hb_buffer_get_glyph_positions(hb_buffer, NULL);
		hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, NULL);
		int z = hb_buffer_get_length(hb_buffer);
		memcpy(pos + len2, x, z * sizeof(*x));

		for (int k = 0; k < z; k++) {
			glyphs[f + k] = info[k].codepoint;
			ax += x[k].x_advance / 64.;
			ay += x[k].y_advance / 64.;
		}

		boundries[lenb].start = len2;
		boundries[lenb].length = z;
		boundries[lenb].ft_face = ft_face;
		boundries[lenb].advance_x = ax;
		boundries[lenb].advance_y = ay;
		++lenb;

		len2 += z;
		f = i;
	}

	for (int i = 0; i < lenb;  ++i) {
		printf("s: %d, l: %d\n", boundries[i].start , boundries[i].length);
	}

	for (int i = 0; i < len2; ++i) {
		double x_advance = pos[i].x_advance / 64.;
		double y_advance = pos[i].y_advance / 64.;
		double x_offset  = pos[i].x_offset / 64.;
		double y_offset  = pos[i].y_offset / 64.;
		printf ("advance=(%g,%g)	offset=(%g,%g)\n", x_advance, y_advance, x_offset, y_offset);
	}

	printf("ax, ay: %lf, %lf\n", ax, ay);

	draw(boundries, lenb, glyphs, len2, ax, FONT_SIZE, pos, j);
	hb_buffer_destroy (hb_buffer);
	FT_Done_FreeType (ft_library);
	return 0;
}
