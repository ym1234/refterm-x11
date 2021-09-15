#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <sys/time.h>
#include <wchar.h>
#include <locale.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);



/* #define CELL_WIDTH 12 */
/* #define CELL_HEIGHT 20 */
/* #define CELL_HEIGHT 12 */
/* #define CELL_WIDTH 6 */

static size_t CELL_WIDTH  = 6;
static size_t CELL_HEIGHT = 20; // set by the user
static size_t ascent = 0; // set by the user
static size_t descent = 0;
static size_t underline_thickness = 0;
static size_t underline = 0;

static FT_Library library;
void die(char *message) {
    printf("%s", message);
    exit(-1);
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ), type, severity, message);
}


char *read_file(char *name) {
    FILE* fd = fopen(name, "r");
    fseek(fd, 0L, SEEK_END);
    int sz = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, sizeof(char), sz, fd);
    buf[sz] = 0;
    /* printf("%s\n", buf); */
    return buf;
}

GLuint compile_shader(char * name, int shadertype) {
    GLuint shader = glCreateShader(shadertype);

    char *buf = read_file(name);
    glShaderSource(shader, 1, &buf, NULL);
    glCompileShader(shader);
    free(buf);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        return 0;
    }
    return shader;
}

GLuint create_program(int num, int *shaders) {
    unsigned int prog = glCreateProgram();
    for (int i = 0; i < num; i++) {
        glAttachShader(prog, shaders[i]);
    }
    glLinkProgram(prog);
    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        printf("%s\n", "Link failure");
        exit(-1);
    }
    return prog;
}

FT_Face init_freetype(int width, int height) {
    /* FT_Library  library; */
    FT_Error error;
    if (error = FT_Init_FreeType(&library)) {
        die("freetype1\n");
    }

    FT_Face face;
    if (error = FT_New_Face(library, "/home/ym/.local/share/fonts/curieMedium.otb", 0, &face)) {
    /* if (error = FT_New_Face(library, "/usr/share/fonts/misc/ter-u28b.otb", 0, &face)) { */
    /* if (error = FT_New_Face(library, "/usr/share/fonts/misc/DinaMedium10.pcf.gz", 0, &face)) { */
    /* if (error = FT_New_Face(library, "/usr/share/fonts/TTF/Roboto-Regular.ttf", 0, &face)) { */
    /* if (error = FT_New_Face(library, "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", 0, &face)) { */
    /* if (error = FT_New_Face(library, "/usr/share/fonts/noto-cjk/NotoSansCJK-Medium.ttc", 0, &face)) { */
        die("freetype2\n");
    }

    if (FT_IS_SCALABLE(face)) {
        if (error = FT_Set_Pixel_Sizes(face, 0, CELL_HEIGHT)) {
            printf("%x\n", error);
            die("freetype3\n");
        }
        /* char *font_format = FT_Get_Font_Format( face ); */

        /* FT_Size_Metrics *size_metrics = &face->size->metrics; */

        CELL_HEIGHT = face->size->metrics.height >> 6;

        if (error = FT_Load_Char(face, 'M', FT_RENDER_MODE_NORMAL)) {
            printf("%x\n", error);
            die("freetype4\n"); /* ignore errors */
        }

        if(error = FT_Render_Glyph( face->glyph,  FT_RENDER_MODE_NORMAL)) {
            printf("%x\n", error);
            die("freetype5\n"); /* ignore errors */
        }

        FT_GlyphSlot slot = face->glyph;
        CELL_WIDTH  = (slot->advance.x >> 6);
        /* printf("w: 0x%x, 0x%x, 0x%x, 0x%x\n", slot->format, slot->bitmap.pixel_mode, slot->bitmap.palette_mode, slot->bitmap.num_grays); */
        /* CELL_WIDTH = face->bbox.xMax >> 6; */
        /* cell_height = face->height; */
        ascent = face->size->metrics.ascender >> 6;
        descent = -face->size->metrics.descender >> 6;
        /* ascent = face->ascender; */
        /* descent = -face->descender; */
        /* CELL_HEIGHT = ascent + descent; */
        printf("height: %d, width: %d\n", CELL_HEIGHT, CELL_WIDTH);
        printf("ascent: %d, descent: %d\n", ascent, descent);
        printf("underline start: %d, underline thickness: %d\n", (-face->underline_position) >> 6, (int) ((float) face->underline_thickness / 64 + 0.5));
        printf("underline start: %d, underline thickness: %d\n", (-face->underline_position), face->underline_thickness);
        underline = (-face->underline_position) >> 6;
        underline_thickness = (int) ((float) face->underline_thickness / 64 + 0.5);
        /* die("test"); */
    }  else  {
        if(!face->available_sizes) {
            die("idfk");
        }
        /* FT_Bitmap_Size size = face->available_sizes[0]; */

        /* CELL_HEIGHT = size.height >> 6; */
        /* CELL_WIDTH = size.width >> 6; */
        /* ascent = face->ascender >> 6; */
        /* descent = -face->descender >> 6; */

        /* CELL_HEIGHT = face->max_advance_height  >> 6; */
        /* CELL_WIDTH = face->max_advance_width >> 6; */
        /* ascent = face->ascender >> 6; */
        /* descent = -face->descender >> 6; */

        /* printf("height: %d, width: %d\n", CELL_HEIGHT, CELL_WIDTH); */
        /* printf("ascent: %d, descent: %d\n", ascent, descent); */
        /* printf("available sizes: %d\n", face->num_fixed_sizes); */

        /* if (error = FT_Set_Pixel_Sizes(face, 0, CELL_HEIGHT)) { */
        for (int i = 0; i < 30; i++){
            if (error = FT_Set_Pixel_Sizes(face, 0, i)) {
                printf("%x\n", error);
                continue;
                die("freetype3\n");
            } else {
                CELL_HEIGHT = face->size->metrics.height >> 6;
                break;
            }
        }

        if (error = FT_Load_Char(face, 'M', FT_RENDER_MODE_NORMAL)) {
            printf("%x\n", error);
            die("freetype4\n"); /* ignore errors */
        }

        if(error = FT_Render_Glyph( face->glyph,  FT_RENDER_MODE_NORMAL)) {
            printf("%x\n", error);
            die("freetype5\n"); /* ignore errors */
        }

        FT_GlyphSlot slot = face->glyph;
        CELL_WIDTH  = slot->advance.x >> 6;
        underline = -1;
        underline_thickness = 1;
    }

    /* printf("z: 0x%x, 0x%x, 0x%x, 0x%x\n", slot->format, slot->bitmap.pixel_mode, slot->bitmap.palette_mode, slot->bitmap.num_grays); */
    /* printf("w: 0x%x, 0x%x, 0x%x\n", target.pixel_mode, target.palette_mode, target.num_grays); */
    /* if (error) { */
    /*     die("freetype5\n"); /1* ignore errors *1/ */
    /* } */
    /* exit(0); */
    return face;
    /* return slot->bitmap; */
}

int handler(Display *d, XErrorEvent ev) {
    printf("Serial: %lu, Error code: %ld\n", ev.serial, ev.error_code);
}

// this is awful
Window create_window(Display *d) {
    static int visual_attribs[] =
    {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        //GLX_SAMPLE_BUFFERS  , 1,
        //GLX_SAMPLES         , 4,
        None
    };

    int fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(d, DefaultScreen(d), visual_attribs, &fbcount);
    if (!fbc)

    {
        printf("Failed to retrieve a framebuffer config\n");
        exit(1);
    }
    printf("Found %d matching FB configs.\n", fbcount);

    // Pick the FB config/visual with the most samples per pixel
    printf("Getting XVisualInfos\n");
    int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

    int i;
    for (i=0; i<fbcount; ++i)
    {
        XVisualInfo *vi = glXGetVisualFromFBConfig(d, fbc[i]);
        if (vi)
        {
            int samp_buf, samples;
            glXGetFBConfigAttrib(d, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
            glXGetFBConfigAttrib(d, fbc[i], GLX_SAMPLES       , &samples );

            printf("  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
                    " SAMPLES = %d\n",
                    i, vi -> visualid, samp_buf, samples);

            if (best_fbc < 0 || samp_buf && samples > best_num_samp)
                best_fbc = i, best_num_samp = samples;
            if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
                worst_fbc = i, worst_num_samp = samples;
        }
        XFree(vi);
    }

    GLXFBConfig bestFbc = fbc[ best_fbc ];

    // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
    XFree(fbc);

    XVisualInfo *vi = glXGetVisualFromFBConfig(d, bestFbc);
    XSetWindowAttributes swa = { .colormap = XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone), .event_mask = ExposureMask | KeyPressMask | StructureNotifyMask, .background_pixmap = None, .border_pixel = 0};

    Window win = XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, 600, 600, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
    if(!win) {
        die("window");
    }
    XFree(vi);

    XStoreName(d, win, "Refterm linux");
    /* XSelectInput(d, win, ExposureMask | KeyPressMask); */

    XMapWindow(d, win);

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");

    int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    printf("Creating context\n");
    GLXContext ctx = glXCreateContextAttribsARB(d, bestFbc, 0, True, context_attribs);
    if(!ctx) {
        die("context");
    }

    glXMakeCurrent(d, win, ctx);

    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    return win;
}

static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";

float *render_ascii(FT_Face face) {
    int rows = CELL_HEIGHT, width = CELL_WIDTH, charnum = strlen(ascii_printable);
    float *data = calloc((rows * 10) * (width * 10) * 4, sizeof(float));

    /* int index  = 4 * (i * width * rows * 10 + j * width + (width - descent) * width * 10 +  k); */
    /* for (int i = 0; i < 10; i++) { */
    /*     for (int k = 0; k < width * 10; k++) { */
    /*         int index = 4 * (i * width * rows * 10 + (rows - descent) * width * 10 + k); */
    /*         data[index]     = 1; */
    /*         data[index + 1] = 1; */
    /*         data[index + 2] = 1; */
    /*         data[index + 3] = 1; */
    /*     } */
    /* } */

/*     for (int i = 1; i < 10; i++) { */
/*         for (int k = 0; k < width * 10; k++) { */
/*             int index = 4 * (i * width * rows * 10 + k); */
/*             data[index]     = 1; */
/*             data[index + 1] = 1; */
/*             data[index + 2] = 1; */
/*             data[index + 3] = 1; */
/*         } */
/*     } */

    for (int i = 0; i < charnum; i++) {
        FT_Error error;

        char *y = "私";
        printf("%x\n", y);
        /* long c = * (long *) y; */
        /* printf("%x\n", c); */
        long c = 0xe7a781;
        printf("%x\n", c);
        FT_UInt glyph_index = FT_Get_Char_Index(face, ascii_printable[i]);
        /* FT_UInt glyph_index = FT_Get_Char_Index(face, 0x79c1); */
        if (error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
            die("freetype4\n"); /* ignore errors */
        }

        if(error = FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_NORMAL)) {
            die("freetype5\n"); /* ignore errors */
        }

        int k = i / 10;
        int f = i % 10;
        FT_Bitmap source = face->glyph->bitmap;
        if (error) {
            die("freetype6\n");
        }

        if (source.pitch == 0) {
            printf("Pitch == 0\n");
            continue;
        }

        FT_Bitmap bitmap;
        if (source.pixel_mode != 0x2) {
            error = FT_Bitmap_Convert(library, &source, &bitmap, 1);
        } else {
            bitmap = source;
        }

        if (bitmap.pitch < 0) { // hack
            printf("pitch less than 0: %d\n", bitmap.pitch);
            bitmap.pitch =  -bitmap.pitch;
            printf("new pitch: %d\n", bitmap.pitch);
        }

        /* printf("pitch: 0x%x, rows: 0x%x, width: 0x%x, 0x%x, 0x%x\n", bitmap.pitch, bitmap.rows, bitmap.width, bitmap.pixel_mode, bitmap.num_grays); */

        int left = face->glyph->bitmap_left;
        /* int left = 0; */
        int top = CELL_HEIGHT - descent - face->glyph->bitmap_top;
        if (bitmap.rows + top >= CELL_HEIGHT) {
            top = 0;
        }
        /* printf("height: %d, bitmap_top: %d, top: %d, descent: %d\n", face->glyph->metrics.height >> 6, face->glyph->bitmap_top, top, descent); */

        int b = i;
        /* printf("left %d, top %d\n", face->glyph->bitmap_left, face->glyph->bitmap_top); */
        for (int i = 0; i < bitmap.rows; i++) {
            for (int j = 0; j < bitmap.width; j++) {
                int index = 4 * (k * (width * 10) * rows + (i + top) * (width * 10) + (j + left) + f * width);
                if (index < 0 || index > (rows * 10) * (width * 10) * 4) {
                    printf("Failed to blit %c\n", ascii_printable[b]);
                    continue;
                }
                float pixel = (float) bitmap.buffer[i * bitmap.pitch + j];
                float num_grays = (float) bitmap.num_grays - 1;
                if (pixel) {
                    /* printf("*"); */
                    float z = pixel / num_grays;
                    data[index] = z;
                    data[index + 1] = z;
                    data[index + 2] = z;
                    data[index + 3] = z;
                } else {
                    /* printf(" "); */
                }
            }
            /* printf("\n"); */
        }
        /* printf("\n"); */
        if (source.pixel_mode != bitmap.pixel_mode) { // is this needed?
            FT_Bitmap_Done(library, &bitmap);
        }
    }
    return data;
}

void clear_screen(unsigned int *screen, int width, int height) {
    unsigned int vertex[4] = { 0, 0, 0x00C5C8C6, 0x1d1f21 };
    /* unsigned int vertex[4] = { 0, 0, 0x00ffffff, 0x1d1f21 }; */
    for (int i = 0; i < width * height * 4; i += 4) {
        int index = i % 4;
        screen[i + 0] = vertex[index];
        screen[i + 1] = vertex[index + 1];
        screen[i + 2] = vertex[index + 2];
        screen[i + 3] = vertex[index + 3];
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, screen);
}

unsigned int *update_screen(int width, int height) {
    unsigned int *my_vertices = calloc(width * height * 4, sizeof(unsigned int));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, width, height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices);
    glUniform2ui(4, width, height); // Term Size
    return my_vertices;
}

char *render(int columns, int rows, unsigned int *screen, char *b) {
    char c;
    int y = 0;
    for (int x = 0; x * y < columns * rows && (c = *b); ++x, ++b) {
        if ((c == '\n') || x / columns) {
            y++;
            if (c == '\n') {
                x = -1;
                continue;
            } else {
                x = 0;
            }
        }
        screen[4 * (y * columns) + 4 * x] = (c - 32) % 10;
        screen[4 * (y * columns) + 4 * x + 1] = (c - 32) / 10;
        /* screen[4 * (y * columns) + 4 * x + 2] |= 0x08000000; */
    }
    return b;
}

int main(void) {
    setbuf(stdout, NULL);
    setlocale(LC_ALL, "C.UTF-8");
    Display *d = XOpenDisplay(NULL);
    if (d == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    Window win = create_window(d);
    FT_Face face = init_freetype(0,0);


    int shaders[2] = { compile_shader("grid.v.glsl", GL_VERTEX_SHADER), compile_shader("grid.f.glsl", GL_FRAGMENT_SHADER) };
    unsigned int prog = create_program(2, shaders);
    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);

    /* float vertices[] = { -1., 1., -1., -1., 1., 1., 1., -1.}; */
    /* float vertices[] = { -1., -1., 3., -1., -1., 3.}; */
    float vertices[] = { -1., 3, -1., -1., 3., -1};

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    glUseProgram(prog);
    glBindVertexArray(VAO);

    glUniform2ui(3, CELL_WIDTH, CELL_HEIGHT); // Cell Size
    glUniform2ui(5, 4, 4); // TopLeftMargin
    /* glUniform2ui(5, 0, 0); // TopLeftMargin */

    glUniform1ui(6, 0xffffffff); // BlinkModulate
    /* glUniform1ui(7, 0x004444); // MarginColor */
    glUniform1ui(7, 0x1d1f21); // MarginColor
    /* glUniform1ui(7, 0xFFFFFFFF); // MarginColor */
    glClearColor(0x1d, 0x1f, 0x21, 0xff);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2ui(8, 16/2 - 16/10, 16/2 + 16/10); // StrikeThrough
    glUniform2ui(9, CELL_HEIGHT - descent + underline,  CELL_HEIGHT - descent + underline + underline_thickness); // underline

    glUniform1i(1, 0); // Glyphs texture
    glUniform1i(2, 1); // Terminal Cells texture

    unsigned int tex[2];

    glGenTextures(2, tex);

    glUniform1i(1, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    float *data = render_ascii(face);
    /* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, face->glyph->bitmap.width * 10, face->glyph->bitmap.rows * 10, 0, GL_RGBA, GL_FLOAT, data); */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CELL_WIDTH * 10, CELL_HEIGHT * 10, 0, GL_RGBA, GL_FLOAT, data);

    glUniform1i(2, 1);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, tex[1]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFinish();


    XWindowAttributes gwa;
    XGetWindowAttributes(d, win, &gwa);
    int columns = (gwa.width - 4) / CELL_WIDTH;
    int rows = (gwa.height - 4) / CELL_HEIGHT;
    unsigned int *my_vertices = update_screen(columns, rows);

    char framerate_c[1000] = {};
    XEvent xev;

    struct timeval time;
    double frametime = 0;
    int framerate = 0;

    int width = gwa.width, height = gwa.height;

    char *screen = calloc(columns * rows, sizeof(char));
    strcpy(screen, "The quick brown fox jumps over the lazy dog");
    int pos = strlen(screen);
    size_t num_frames = 1;
    double frametime_total = 0;
    size_t sum_fps = 0;
    while(1) {
        gettimeofday(&time, NULL);
        long start_time = 1000000 * time.tv_sec + time.tv_usec;

        int pending = XPending(d);
        while (pending) {
            XNextEvent(d, &xev);
            if (xev.type == ConfigureNotify) {
                XGetWindowAttributes(d, win, &gwa);
                if (gwa.width == width && gwa.height == height) {
                    pending--;
                    continue;
                }
                int new = ((gwa.width - 4) / CELL_WIDTH) *  ((gwa.height - 4) / CELL_HEIGHT);
                screen = realloc(screen,  new);
                if (columns * rows < new) {
                    for (int i = columns * rows; i < new; i++) {
                        screen[i] = 0;
                    }
                }
                columns = (gwa.width - 4) / CELL_WIDTH;
                rows = (gwa.height - 4) / CELL_HEIGHT;
                free(my_vertices);
                my_vertices = update_screen(columns, rows);
                width = gwa.width, height = gwa.height;
                glViewport(0, 0, gwa.width, gwa.height);
            } else if (xev.type = KeyPress) {
                char *b[36] = {};
                KeySym keysym;
                XLookupString(&xev.xkey, b, sizeof(b), &keysym, NULL);
                /* printf("%x\n", keysym); */
                if (b[0] >= 32 && b[0] <= 32 + 95 && strlen(b) == 1) {
                    screen[pos++] = b[0];
                } else if (keysym == 0xff0du) {
                    screen[pos++] = '\n';
                } else if (keysym == 0xff08) {
                    if (pos > 0) {
                        --pos;
                    }
                }
                screen[pos] = '|';
                screen[pos + 1] = '\0';
            }
            pending--;
        }

        clear_screen(my_vertices, columns, rows);
        char *c = render(columns, rows, my_vertices, screen);

        snprintf(framerate_c, 1000, "FPS: %5d, mSPF: %f, aFPS: %f, amSPF %f, columns: %d, rows: %d", framerate, frametime * 100, (double) sum_fps / num_frames, (double) frametime_total / num_frames, columns, rows);
        size_t framelen = strlen(framerate_c);

        for (int i = 0; i < framelen; i++) {
            my_vertices[columns * rows * 4 - 4 * (framelen - i)] = (framerate_c[i] - 32) % 10;
            my_vertices[columns * rows * 4 - 4 * (framelen - i) + 1] = (framerate_c[i] - 32) / 10;
        }
        /* glTexSubImage2D(GL_TEXTURE_2D, 0, columns - framelen, rows - 1, framelen, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices + (columns * rows * 4 - 4 * framelen)); */
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, columns, rows, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices);

        {
            /* glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); */
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glXSwapBuffers(d, win);
        }

        gettimeofday(&time, NULL);
        long end_time = 1000000 * time.tv_sec + time.tv_usec;
        frametime = ((double)(end_time - start_time) / 1000000);
        framerate = (int) (1 / frametime);
        sum_fps += framerate;
        frametime_total += frametime;
        ++num_frames;
        /* printf("average frame rate %f\n", (float) sum_fps / num_frames); */
        /* break; */
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(prog);

    XCloseDisplay(d);

    return 0;
}
