#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

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
    printf("%s\n", buf);
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
    FT_Library  library;
    FT_Error error;
    if (error = FT_Init_FreeType(&library)) {
        die("freetype1\n");
    }

    FT_Face face;
    if (error = FT_New_Face(library, "/home/ym/.local/share/fonts/curieMedium.otb", 0, &face)) {
    /* if (error = FT_New_Face(library, "/usr/share/fonts/noto/NotoSansMono-Medium.ttf", 0, &face)) { */
        die("freetype2\n");
    }
    if (error = FT_Set_Pixel_Sizes(face, 0, 12)) {
        printf("%x\n", error);
        die("freetype3\n");
    }

    if (error = FT_Load_Char(face, 'F', FT_RENDER_MODE_NORMAL)) {
        printf("%x\n", error);
        die("freetype4\n"); /* ignore errors */
    }
    printf("x advance: %d\n", face->glyph->advance.x);

    if(error = FT_Render_Glyph( face->glyph,  FT_RENDER_MODE_NORMAL)) {
        printf("%x\n", error);
        die("freetype5\n"); /* ignore errors */
    }

    FT_GlyphSlot slot = face->glyph;
    printf("w: 0x%x, 0x%x, 0x%x, 0x%x\n", slot->format, slot->bitmap.pixel_mode, slot->bitmap.palette_mode, slot->bitmap.num_grays);
    /* printf("z: 0x%x, 0x%x, 0x%x, 0x%x\n", slot->format, slot->bitmap.pixel_mode, slot->bitmap.palette_mode, slot->bitmap.num_grays); */
    return face;
    /* return slot->bitmap; */
}

int handler(Display *d, XErrorEvent ev) {
    printf("Serial: %lu, Error code: %ld\n", ev.serial, ev.error_code);
}

// This is awful
Window create_window(Display *d) {
        clock_t start_time = clock();
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

    XStoreName(d, win, "GL 3.0 Window");
    /* XSelectInput(d, win, ExposureMask | KeyPressMask); */

    XMapWindow(d, win);

    /* XSync(d, False); */

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
    /* XSync(d, False); */
    /* glXMakeCurrent(d, win, glXCreateContext(d, vi, NULL, GL_TRUE)); */
    glXMakeCurrent(d, win, ctx);

    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Created context in %f seconds\n", elapsed_time);

    return win;
}

static char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";

float *render_ascii(FT_Face face) {
    double start_time = clock();
    int rows = 12, width = 6, charnum = strlen(ascii_printable);
    printf("charnum: %d\n", charnum);
    float *data = calloc((rows * 10) * (width * 10) * 4, sizeof(float));

    for (int i = 0; i < charnum; i++) {
        FT_Error error;
        if (error = FT_Load_Char(face, ascii_printable[i], FT_RENDER_MODE_NORMAL)) {
            die("freetype4\n"); /* ignore errors */
        }

        if(error = FT_Render_Glyph(face->glyph,  FT_RENDER_MODE_NORMAL)) {
            die("freetype5\n"); /* ignore errors */
        }

        int k = i / 10;
        int f = i % 10;
        FT_Bitmap bitmap = face->glyph->bitmap;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < width; j++) {
                int index = 4 * (k * (width * 10) * rows +  f * width + i * (width * 10) + j);
                if ((bitmap.buffer[i] & (0x80 >> j))) {
                    data[index] = 1.;
                    data[index + 1] = 1.;
                    data[index + 2] = 1.;
                    data[index + 3] = 1.;
                } else {
                    /* data[index + 3] = 1.; */
                    /* data[index + 2] = 0.26f; */
                    /* data[index + 1] = 0.26f; */
                    /* data[index + 2] = 0.1294; */
                    /* data[index + 1] = 0.12156; */
                    /* data[index] = 0.1137; */
                }
            }
        }
    }
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Rendered ascii in %f seconds\n", elapsed_time);
    return data;
}

int main(void) {
    Display *d = XOpenDisplay(NULL);
    if (d == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    Window win = create_window(d);
    FT_Face face = init_freetype(0,0);
    /* printf("pitch: %d, row: %d, width: %d\n", bitmap.pitch, bitmap.rows, bitmap.width); */
    /* for (int i = 0; i < bitmap.rows; i++) { */
    /*     /1* printf("0x%x", bitmap.buffer[i]); *1/ */
    /*     for (int j = 0; j < bitmap.width; j++) { */
    /*         /1* if (bitmap.buffer[i * bitmap.width + j]) { *1/ */
    /*         /1*     printf("*"); *1/ */
    /*         /1* } else { *1/ */
    /*         /1*     printf(" "); *1/ */
    /*         /1* } *1/ */
    /*         if ((bitmap.buffer[i] & (0x80 >> j))) { */
    /*             printf("*"); */
    /*         } else { */
    /*             printf(" "); */
    /*         } */
    /*     } */
    /*     printf("\n"); */
    /* } */

    /* float *data = calloc(bitmap.rows * bitmap.width * 4 * 100, sizeof(float)); */
    /* float *data = calloc(bitmap.rows * bitmap.width * 4, sizeof(float)); */
    /* float *data = malloc(289 * sizeof(float)); */
    /* for (int i = 0; i < 289; i++) { */
    /*     *data++ = 1.; */
    /* } */


    /* for (int i = 0; i < bitmap.rows; i++) { */
    /*     for (int j = 0; j < bitmap.width; j++) { */
    /*         int index =  4 * (i * bitmap.width + j); */
    /*         if ((bitmap.buffer[i] & (0x80 >> j))) { */
    /*             data[index] = 1.; */
    /*             data[index + 1] = 1.; */
    /*             data[index + 2] = 1.; */
    /*             data[index + 3] = 1.; */
    /*         } else { */
    /*             data[index + 3] = 1.; */
    /*             /1* data[index + 2] = 0.26f; *1/ */
    /*             /1* data[index + 1] = 0.26f; *1/ */
    /*             data[index + 2] = 0.1294; */
    /*             data[index + 1] = 0.12156; */
    /*             data[index] = 0.1137; */
    /*         } */
    /*     } */
    /* } */

    /* for (int k = 0; k < 10; k++) { */
    /*     for (int i = 0; i < bitmap.rows; i++) { */
    /*         for (int j = 0; j < bitmap.width * 10; j++) { */
    /*             /1* int index =  k * 4 * (bitmap.width * 10) * bitmap.rows + 4 * ((bitmap.rows - i - 1) * (bitmap.width * 10) + j); *1/ */
    /*             int index =  k * 4 * (bitmap.width * 10) * bitmap.rows + 4 * (i * (10 * bitmap.width) + j); */
    /*             int k = j % bitmap.width; */
    /*             if ((bitmap.buffer[i] & (0x80 >> k))) { */
    /*             /1* if ((bitmap.buffer[i * bitmap.width + k])) { *1/ */
    /*                 data[index] = 1.; */
    /*                 data[index + 1] = 1.; */
    /*                 data[index + 2] = 1.; */
    /*                 data[index + 3] = 1.; */
    /*             } else { */
    /*                 data[index + 3] = 1.; */
    /*                 /1* data[index + 2] = 0.26f; *1/ */
    /*                 /1* data[index + 1] = 0.26f; *1/ */
    /*                 data[index + 2] = 0.1294; */
    /*                 data[index + 1] = 0.12156; */
    /*                 data[index] = 0.1137; */
    /*             } */
    /*         } */
    /*     } */
    /* } */

    clock_t start_time = clock();
    int shaders[2] = { compile_shader("grid.v.glsl", GL_VERTEX_SHADER), compile_shader("grid.f.glsl", GL_FRAGMENT_SHADER) };
    unsigned int prog = create_program(2, shaders);
    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);
    double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Compilied shaders in %f seconds\n", elapsed_time);

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

    glUniform2ui(3, 6, 12); // Cell Size
    glUniform2ui(4, 25, 4); // Term Size
    glUniform2ui(5, 4, 4); // TopLeftMargin
    /* glUniform2ui(5, 0, 0); // TopLeftMargin */

    glUniform1ui(6, 0xffffffff); // BlinkModulate
    /* glUniform1ui(7, 0x004444); // MarginColor */
    glUniform1ui(7, 0x1d1f21); // MarginColor
    /* glUniform1ui(7, 0xFFFFFFFF); // MarginColor */

    glUniform2ui(8, 16/2 - 16/10, 16/2 + 16/10); // StrikeThrough
    glUniform2ui(9, 16 - 16/5, 16); // underline

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, face->glyph->bitmap.width * 10, face->glyph->bitmap.rows * 10, 0, GL_RGBA, GL_FLOAT, data);

    glUniform1i(2, 1);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, tex[1]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    unsigned int *my_vertices = calloc(4 * 10 * 10, sizeof(unsigned int));
    unsigned int vertex[4] = { 0, 0, 0x00C5C8C6, 0x1d1f21 };
    for (int i = 0; i < 4 * 10 * 10; i++) {
        my_vertices[i] = vertex[i % 4];
    /*     if(i % 4 == 0) { */
    /*         my_vertices[i] = (i % 40) / 4; */
    /*     } */
    /*     if((i - 1) % 4 == 0) { */
    /*         my_vertices[i] = i / 40; */
    /*     } */
    }

    for (int i = 0; i < 100; i += 1) {
        my_vertices[i * 4 + 1] = i / 10;
        my_vertices[i * 4] = i % 10;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 25, 4, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices);
    /* glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, vertex); */

    XEvent xev;
    XWindowAttributes gwa;
    while(1) {

        int pending = XPending(d);
        while (pending) {
            XNextEvent(d, &xev);
            if (xev.type == ConfigureNotify) {
                XGetWindowAttributes(d, win, &gwa);
                glViewport(0, 0, gwa.width, gwa.height);
            }
            pending--;
        }
        clock_t start_time = clock();
        {
            /* glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); */
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glXSwapBuffers(d, win);
        }
        double elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        printf("Done in %f seconds\n", elapsed_time);
        /* break; */
        usleep(16000);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(prog);

    XCloseDisplay(d);

    return 0;
}
