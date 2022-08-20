#define _GNU_SOURCE
/* #define TRACY_ENABLE */
/* #define TRACY_DEBUGINFOD */

#include "/home/ym/fun/tracy/public/tracy/TracyC.h"
#include <sys/mman.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <wchar.h>
#include <locale.h>
#include <stdarg.h>
#include <pty.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>
#include <pwd.h>
#include <X11/Xatom.h>
#include <poll.h>

void die(char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	/* exit(-1); */
}

long long get_time(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW,&ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}

#include "freetype.c"
#include "ogl.c"
#include "circular_buffer.c"
#include "tty.c"
#include "window.c"

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
	unsigned int *v = calloc(width * height * 4, sizeof(*v));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, width, height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, v);
	glUniform2ui(4, width, height); // Term Size
	return v;
}

char *render(int columns, int rows, unsigned int *screen, unsigned int *b, int cur_x, int cur_y) {
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < columns; ++j) {
			unsigned int c = (char) *(b++);
			if (!c) {
				continue;
			}
			int z = 4 * ((i * columns) + j);
			screen[z] = (c - 32) % 10;
			screen[z + 1] = (c - 32) / 10;
		}
	}

	screen[4 * (cur_y * columns + cur_x)] = ('~' - 31) % 10;
	screen[4 * (cur_y * columns + cur_x) + 1] = ('~' - 31) / 10;

	return "";
	/* return b; */
}


void set_props(FTFont f) {
	glUniform2ui(3, f.cellwidth, f.cellheight); // Cell Size
	glUniform2ui(5, 4, 4); // TopLeftMargin
	/* glUniform2ui(5, 0, 0); // TopLeftMargin */

	glUniform1ui(6, 0xffffffff); // BlinkModulate
	glUniform1ui(7, 0x1d1f21); // MarginColor
	glClearColor(0x1d, 0x1f, 0x21, 0xff);
	glClear(GL_COLOR_BUFFER_BIT);
	glUniform2ui(8, 16/2 - 16/10, 16/2 + 16/10); // StrikeThrough
	glUniform2ui(9, f.cellheight - f.descent + f.underline - (int)(f.underline_thickness  / 2.0 + 0.5),  f.cellheight - f.descent + f.underline - (int)(f.underline_thickness  / 2.0 + 0.5) + f.underline_thickness); // underline
	/* glUniform2ui(9, 10, 2); */
}

void prep_tex(unsigned int tex, int texnum, int attridx) {
	glUniform1i(attridx, texnum);
	glActiveTexture(GL_TEXTURE0 + texnum);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

OGLContext prepare_glcontext() {
	OGLContext ctx = {};
	int shaders[2] = { compile_shader("grid.v.glsl", GL_VERTEX_SHADER), compile_shader("grid.f.glsl", GL_FRAGMENT_SHADER) };
	ctx.prog  = create_program(2, shaders);
	for (int i = 0; i < 2; ++i) {
		glDeleteShader(shaders[i]);
	}

	float vertices[] ={ -1., 3, -1., -1., 3., -1 };
	memcpy(ctx.vertices, vertices, sizeof(float) * 6);

	glGenVertexArrays(1, &ctx.VAO);
	glBindVertexArray(ctx.VAO);
	glGenBuffers(1, &ctx.VBO);

	glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glUseProgram(ctx.prog);
	glBindVertexArray(ctx.VAO);

	glGenTextures(2, ctx.tex);
	prep_tex(ctx.tex[0], 0, 2);
	prep_tex(ctx.tex[1], 1, 1);

	glFinish();
	return ctx;
}

void clean_glcontext(OGLContext ctx) {
	glDeleteVertexArrays(1, &ctx.VAO);
	glDeleteBuffers(1, &ctx.VBO);
	glDeleteProgram(ctx.prog);
}

void print_array(char **a, int len) {
	for (int i = 0; i < len; ++i) {
		printf("%s, ", a[i]);
	}
	printf("\n");
}
int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
    for (int opt; (opt = getopt(argc, argv, "")) != -1; ) { switch(opt) { } } // TODO(ym):

	char **args = NULL;
	if (optind < argc) {
		args = &argv[optind];
		printf("Non-option args: ");
		print_array(argv + optind, argc - optind);
	}

	if (!args) {
		args = &(char *){"/bin/bash"}; // NOTE: C99
	}

	Display *d = XOpenDisplay(NULL);

#if 0
	int pagesize = getpagesize();
#else
	int pagesize = sysconf(_SC_PAGESIZE);
#endif

#define IsPowerOfTwo(x) (((x) & (x - 1)) == 0)
	assert(IsPowerOfTwo(pagesize));

	Terminal t = create_terminal(d, pagesize * 20, args);

	map_window(&t.xw);
	if (!gladLoadGL()) die("Couldn't load opengl\n"); // TODO(ym): Check if glad is already loaded
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

	t.gh = 12;
	t.gw = 0;

	FT_Library ftlib;
	if(FT_Init_FreeType(&ftlib))
		die("Couldn't load freetype");

	FTFont f = load_font(ftlib, "/home/ym/.local/share/fonts/curieMedium.otb", t.gw, t.gh);
	t.gw = f.cellwidth;

	set_terminalsz(&t);

	OGLContext ctx = prepare_glcontext();
	set_props(f);

	float *data = render_ascii(f, ftlib);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, f.cellwidth * 10, f.cellheight * 10, 0, GL_RGBA, GL_FLOAT, data);

	glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 0, 0, 0, GL_RGBA, GL_FLOAT, (float *) NULL);

	char x[0x1000] = {};
	int xfd = XConnectionNumber(d);
	XEvent xev;
	while (1) {
		TracyCFrameMarkStart("Frame");

		fd_set y;
		FD_ZERO(&y);
		FD_SET(t.cmdfd, &y);
		FD_SET(xfd, &y);

		float timeout = 16; // ms
		if (XPending(d))
			timeout = 0;  // existing events might not set xfd

		struct timespec ts = { .tv_sec = timeout / 1E3, .tv_nsec = 1E6 * (timeout - 1E3 * ts.tv_sec) };

		// Defined in TracyC.h
#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif

		{
			TracyCZoneN(ctx, "PSelect", 1);
			if (pselect(MAX(xfd, t.cmdfd)+1, &y, NULL, NULL, &ts, NULL) < 0) {
				if (errno == EINTR)
					continue;
				die("select failed: %s\n", strerror(errno));
			}
			TracyCZoneEnd(ctx);
		}

		if (XPending(d)) {
			TracyCZoneN(ctx, "X11 code", 1);
			XNextEvent(d, &xev);
			if (handler[xev.type]) {
				handler[xev.type](&t, &xev);
			}
			TracyCZoneEnd(ctx);
		}

		if (FD_ISSET(t.cmdfd, &y)) {
			TracyCZoneN(ctx, "Reading loop", 2);
			int r = read(t.cmdfd, x, 0x1000);
			TracyCZoneEnd(ctx);
		}

		{
			TracyCZoneN(ctx, "drawing loop", 3);
			/* clear_screen(v, sz.x, sz.y); */
			/* render(sz.x, sz.y, v, ms, c); */

			/* glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sz.x, sz.y, GL_RGBA_INTEGER, GL_UNSIGNED_INT, v); */

			{
				glDrawArrays(GL_TRIANGLES, 0, 3);
				glFinish();
				glXSwapBuffers(d, t.xw.win);
				/* XFlush(d); */
			}
			TracyCZoneEnd(ctx);
		}

		TracyCFrameMarkEnd("Frame");

	}

	clean_glcontext(ctx);
	XCloseDisplay(d);
}

#if 0
int main(int argc, char *argv[]) {

	char *cmd = NULL;

    for (int opt; (opt = getopt(argc, argv, "")) != -1; ) {
        switch(opt) {
			/* case 'c': */
			/* 	cmd = optarg; */
			/* 	break; */
        }
    }

	char **args;
	if (optind < argc)
	{
		args = &argv[optind];
		printf("Non-option args: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}

	/* setbuf(stdout, NULL); */
	/* setlocale(LC_ALL, "C.UTF-8"); */

	create_cbuf(4096);

	printf("print: %d\n", print);
	/* printf("cmd: %s\n", cmd); */

	int cmdfd = ttynew("/bin/bash", args);

	/* int flags = fcntl(cmdfd, F_GETFL, 0); */
	/* fcntl(cmdfd, F_SETFL, flags | O_NONBLOCK); */
	Display *d = XOpenDisplay(NULL);
	if (d == NULL) {
		fprintf(stderr, "Cannot open display\n");
		exit(-1);
	}

	Vec wz = {};
	{
		XEvent xev;
		do {
			XNextEvent(d, &xev);
			if (XFilterEvent(&xev, None)) {
				continue;
			} else if (xev.type == ConfigureNotify) {
				wz.x = xev.xconfigure.width;
				wz.y = xev.xconfigure.height;
				printf("%d, %d\n", wz.x, wz.y);
			}
		} while (xev.type != MapNotify);
	}

	FT_Library library;
	if(FT_Init_FreeType(&library)) {
		die("Couldn't load freetype");
	}
	FTFont f = load_font(library, "/home/ym/.local/share/fonts/curieMedium.otb", 0, 12);
	/* FTFont f = load_font(library, "/usr/share/fonts/misc/ter-u28b.otb", 0, 16); */
	/* FTFont font = load_font(library, "/usr/share/fonts/liberation/LiberationMono-Regular.ttf", 0, 16); */
	/* FTFont font = load_font(library, "/usr/share/fonts/TTF/Roboto-Regular.ttf", 0, 16); */
	/* FTFont f = load_font(library, "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", 0, 16); */

	Vec sz = screensize(wz, f);
	unsigned int *v = update_screen(sz.x, sz.y);

#define SIZE 0x1000
	char x[SIZE] = {0};

	unsigned int *ms = calloc(sz.x * sz.y, sizeof(*ms));
	Vec c = {0};

	XEvent xev;
	int xfd = XConnectionNumber(d);

	fd_set y;
	while(1) {
		TracyCFrameMarkStart("Frame");

		FD_ZERO(&y);
		FD_SET(cmdfd, &y);
		FD_SET(xfd, &y);


		float timeout = 16; //ms
		if (XPending(d))
			timeout = 0;  /* existing events might not set xfd */

		struct timespec ts = { .tv_sec = timeout / 1E3, .tv_nsec = 1E6 * (timeout - 1E3 * ts.tv_sec) };

#define MAX(a, b) ((a) < (b) ? (b) : (a))

		{
			TracyCZoneN(ctx, "PSelect", 1);
			if (pselect(MAX(xfd, cmdfd)+1, &y, NULL, NULL, &ts, NULL) < 0) {
				if (errno == EINTR)
					continue;
				die("select failed: %s\n", strerror(errno));
			}
			TracyCZoneEnd(ctx);
		}

		if (XPending(d)) {
			TracyCZoneN(ctx, "X11 code", 1);
			while (XPending(d)) {
				XNextEvent(d, &xev);
				switch (xev.type) {
					case ButtonPress: {
					} break;
					case ConfigureNotify: {
						Vec wz_t = {.x = xev.xconfigure.width, .y = xev.xconfigure.height };
						if (wz.x == wz_t.x && wz.y == wz_t.y) {
							continue;
						}

						wz = wz_t;
						sz = screensize(wz, f);
						free(v);
						free(ms);

						ms = calloc(sz.x * sz.y, sizeof(*ms));
						v = update_screen(sz.x, sz.y);

						glScissor(0, 0, wz.x, wz.y);
						glEnable(GL_SCISSOR_TEST);
						glViewport(0, 0, wz.x, wz.y);
					} break;
					case KeyPress: {
						char b[36] = {};
						KeySym keysym;
						int l = XLookupString(&xev.xkey, b, sizeof(b), &keysym, NULL);
						printf("B: ");
						for (int i = 0; i < l; i++) {
							printf("%x, ", b[i]);
						}
						printf("\n");
						if (l == 1 && b[0] < 0x80) {
							if (b[0] == '\r') {
								write(cmdfd, "\r\n", 2);
							}
							write(cmdfd, b, l);
						} else if (keysym == 0xff52) {
							char *s = "\033[A\0";
							write(cmdfd, s, strlen(s));
						} else {
							printf("Unknown keycode received: %x\n", keysym);
						}
					} break;

					case ClientMessage: {
						printf("Received client message\n");
						if ((Atom)xev.xclient.data.l[0] == atom) {
							return 0;
							/* exit(0); */
						}
					} break;
				}
			}
			TracyCZoneEnd(ctx);
		}

		if (FD_ISSET(cmdfd, &y)) {
			TracyCZoneN(ctx, "Reading loop", 2);
			int r = read(cmdfd, x, SIZE);
			TracyCZoneEnd(ctx);
		}

		{
			TracyCZoneN(ctx, "drawing loop", 3);
			clear_screen(v, sz.x, sz.y);
			render(sz.x, sz.y, v, ms, c);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sz.x, sz.y, GL_RGBA_INTEGER, GL_UNSIGNED_INT, v);

			{
				glDrawArrays(GL_TRIANGLES, 0, 3);
				glFinish();
				glXSwapBuffers(d, win);
				XFlush(d);
			}
			TracyCZoneEnd(ctx);
		}

		TracyCFrameMarkEnd("Frame");
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(prog);

	XCloseDisplay(d);

	return 0;
}
#endif
