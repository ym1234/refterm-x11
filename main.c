#define _GNU_SOURCE
#define TRACY_ENABLE
#define TRACY_DEBUGINFOD

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

// Defined in TracyC.h
#ifndef MAX
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif
#ifndef MIN

#define MAX(a, b) ((a) > (b) ? (b) : (a))
#endif

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

long getps(void) {
#if 0
	return getpagesize();
#else
	return sysconf(_SC_PAGESIZE);
#endif
}

#include "refterm_example_source_buffer.h"
#include "refterm_example_source_buffer.c"
#include "freetype.c"
#include "ogl.c"
#include "tty.c"
#include "window.c"

#define printf(...)
void clear_screen(unsigned int *screen, int width, int height) {
	TracyCZoneN(ctx, "clearing screen", true);
	/* int s = ARRSIZE(ascii_printable); */
	unsigned int vertex[4] = { 0, 0, 0x00C5C8C6, 0x1d1f21 };
	/* unsigned int vertex[4] = { 0, 0, 0, 0 }; */
	for (int i = 0; i < width * height * 4; i += 4) {
		int index = i % 4;
		/* unsigned int k = 10 * (rand() / ((float) RAND_MAX)); */
		/* unsigned int v = 10 * (rand() / ((float) RAND_MAX)); */
		screen[i + 0] = vertex[index];
		screen[i + 1] = vertex[index + 1];
		screen[i + 2] = vertex[index + 2];
		screen[i + 3] = vertex[index + 3];
	}
	TracyCZoneEnd(ctx);
	/* glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, screen); */
}

size_t CurrentLine(Terminal *t) {
	return t->CurrentLine % t->th;
}

void AdvanceRow(Terminal *t) {
	t->CurrentLineWidth = 0;
	t->CurrentLine++;

	if (t->CurrentLine - t->AnchorLine >= t->th) {
		size_t x = (t->CurrentLine - t->AnchorLine) - t->th + 1;
		/* printf("x: %d, currentline - anchorline: %d\n", x, t->CurrentLine - t->AnchorLine); */
		for (int i = 0; i < x; i++) {
			t->lines[(t->AnchorLine + i) % t->th] = 0;
			t->offsets[(t->AnchorLine + i) % t->th] = 0;
		}
		t->AnchorLine += x;
		printf("Current Anchor: %d\n", t->AnchorLine);
	}
	t->lines[CurrentLine(t)] = t->SegmentCount;
	t->offsets[CurrentLine(t)] = 0;
}

void AdvanceColumn(Terminal *t) {
	t->CurrentLineWidth++;
	if (t->CurrentLineWidth >= t->tw) {
		AdvanceRow(t);
		t->lines[CurrentLine(t)] = t->SegmentCount - 1;
		t->offsets[CurrentLine(t)] = t->sg[t->SegmentCount - 1].buf.Count;
	}
}

void SetCell(Terminal *t, int x, int y, char ch) {
	if (ch < 32 || ch > 126) {
		ch = '?';
	}
	int idx = 4 * (y * t->tw + x);
	t->screen[idx] = (ch - 32) % 10;
	t->screen[idx + 1] = (ch - 32) / 10;
}

void ProcessInput(Terminal *t, TCursor *cursor, source_buffer_range ob) {

	TracyCZoneN(ctx, "processing input", true);
	assert(t->sg);

	source_buffer_range buf = ob;
	int rows_moved = 0;
	while (buf.Count) {
		size_t nsg = t->SegmentCount++;

		t->sg[nsg].buf = buf;
		t->sg[nsg].buf.Count = 0;

		for (int i = 0; i < buf.Count; ++i) {
			if (buf.Data[i] == '\r' || buf.Data[i] == '\n') { //  || !(buf.Count - i - 1)) {
				if (buf.Data[i] == '\r' && i + 1 < buf.Count && buf.Data[i+1] == '\n') {
					buf = ConsumeCount(buf, i + 2);
				} else if (buf.Data[i] == '\n') {
					buf = ConsumeCount(buf, i + 1);
				} else if (buf.Data[i] == '\r') {
					buf.Count = 0;
					break;
				}
				t->sg[nsg].line_end = true;
				AdvanceRow(t);
				break;
			}
			t->sg[nsg].buf.Count++;
			AdvanceColumn(t);
			if (i == (buf.Count - 1)) {
				buf = ConsumeCount(buf, buf.Count);
			}
		}
	}
	TracyCZoneEnd(ctx);
}

void set_props(FTFont f) {
	glUniform2ui(3, f.cellwidth, f.cellheight); // Cell Size
	glUniform2ui(5, 4, 4); // TopLeftMargin

	glUniform1ui(6, 0xffffffff); // BlinkModulate
	glUniform1ui(7, 0x280418); // 0x1d1f21); // MarginColor

	glUniform2ui(8, 16/2 - 16/10, 16/2 + 16/10); // StrikeThrough
	glUniform2ui(9, f.cellheight - f.descent + f.underline - (int)(f.underline_thickness  / 2.0 + 0.5),  f.cellheight - f.descent + f.underline - (int)(f.underline_thickness  / 2.0 + 0.5) + f.underline_thickness); // underline
	/* glUniform2ui(9, 10, 2); */
}

void Render(Terminal *t) {
	TracyCZoneN(ctx, "rendering input", true);
	printf("Current Anchor in render: %d\n", t->AnchorLine);
	int CursorY = 0;

	for (int i = 0; i < t->CurrentLine - t->AnchorLine + 1; ++i) {
		size_t line = t->lines[(t->AnchorLine + i) % t->th];
		size_t offset = t->offsets[(t->AnchorLine + i) % t->th];
		printf("loop n#%d: %ld, %ld\n", i, line, offset);
		if (line == ((size_t) -1)) break;

		int chrprinted = 0;// offset;
		Segment *current_segment = &t->sg[line];
		source_buffer_range buf = ConsumeCount(current_segment->buf, offset);
		while (buf.Count) {
			for (int j = 0; j < buf.Count; ++j) {
				char ch = buf.Data[j];
				SetCell(t, chrprinted + j, i, ch);
			}
			chrprinted += buf.Count;
			if (current_segment->line_end) break;
			if (chrprinted >= t->tw) break;
			current_segment++;
			buf = current_segment->buf;
		}
	}
	printf("\n");
	TracyCZoneEnd(ctx);
	return;
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

void prepare_glcontext(Terminal *t) {
	int shaders[2] = { compile_shader("grid.v.glsl", GL_VERTEX_SHADER), compile_shader("grid.f.glsl", GL_FRAGMENT_SHADER) };
	t->glres.program  = create_program(2, shaders);
	for (int i = 0; i < 2; ++i) {
		glDeleteShader(shaders[i]);
	}

	glUseProgram(t->glres.program);

	float vertices[] ={ -1., 3, -1., -1., 3., -1 };
	memcpy(t->glres.vertices, vertices, sizeof(float) * 6);

	glGenVertexArrays(1, &t->glres.VAO);
	glBindVertexArray(t->glres.VAO);
	glGenBuffers(1, &t->glres.VBO);

	glBindBuffer(GL_ARRAY_BUFFER, t->glres.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glGenTextures(1, &t->glres.glyph_tex);
	glGenTextures(1, &t->glres.cell_tex);
	t->glres.gt_slot = 0;
	t->glres.ct_slot = 1;
	prep_tex(t->glres.glyph_tex, t->glres.gt_slot, 1);
	prep_tex(t->glres.cell_tex, t->glres.ct_slot, 2);

	glEnable(GL_SCISSOR_TEST);

	/* glFinish(); */
}

/* void clean_glcontext(OGLContext ctx) { */
/* 	glDeleteVertexArrays(1, &ctx.VAO); */
/* 	glDeleteBuffers(1, &ctx.VBO); */
/* 	glDeleteProgram(ctx.prog); */
/* } */

#undef printf
int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	for (int opt; (opt = getopt(argc, argv, "")) != -1; ) { switch(opt) { } } // TODO(ym):

	char **args = NULL;
	if (optind < argc) {
		args = &argv[optind];
		for (char **i = argv + optind; i < argv + argc; ++i) {
			printf("%s, ", *i);
		}
		printf("\n");
	}

	if (!args) {
		args = &(char *){"/bin/bash"}; // NOTE: C99
	}

	Display *d = XOpenDisplay(NULL);

	Terminal t = create_terminal(d, getps() * 256 * 2, args);
	/* fcntl(t.cmdfd, F_SETFL, O_NONBLOCK); */
	/* printf("fcntl return type %d\n", ); */
	/* if (= -1) */
	/* 	printf("O_NONBLOCK FAILED\n"); */

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

	prepare_glcontext(&t);
	set_props(f);
	glDisable(GL_MULTISAMPLE);

	map_window(&t);
	set_terminalsz(&t);


	float *data = render_ascii(f, ftlib);
	glActiveTexture(GL_TEXTURE0 + t.glres.gt_slot);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, f.cellwidth * 10, f.cellheight * 10, 0, GL_RGBA, GL_FLOAT, data);

	glActiveTexture(GL_TEXTURE0 + t.glres.ct_slot);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, t.tw, t.th, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, (unsigned int *) NULL);

	int xfd = XConnectionNumber(d);
	XEvent xev;
	t.cmdfd = ttynew("/bin/bash", args);
	t.lines[0] = 0;
	/* fcntl(t.cmdfd, F_SETFL, O_NONBLOCK); */
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

		{
			TracyCZoneN(ctx, "PSelect", true);
			if (pselect(MAX(xfd, t.cmdfd)+1, &y, NULL, NULL, &ts, NULL) < 0) {
				if (errno == EINTR)
					continue;
				die("select failed: %s\n", strerror(errno));
			}
			TracyCZoneEnd(ctx);
		}

		{
			TracyCZoneN(ctx, "X11 code", true);
			while (XPending(d)) {
				XNextEvent(d, &xev);
				if (XFilterEvent(&xev, None)) continue;
				if (handler[xev.type]) {
					handler[xev.type](&t, &xev);
				}
			}
			TracyCZoneEnd(ctx);
		}

		if (FD_ISSET(t.cmdfd, &y)) {
			TracyCZoneN(ctx, "Reading loop", true);
			int r = 0;
			long long ts = get_time();
			long long te = ts;
			long num_reads = 0;
			source_buffer_range x = GetNextWritableRange(&t.buf, LARGEST_AVAILABLE);
			/* printf("x.Count: %d\n", x.Count); */
			long rd = 0;


			TCursor c = {};
			fd_set z;
			FD_ZERO(&z);
			FD_SET(t.cmdfd, &z);
			struct timespec tz = { .tv_sec = 0, .tv_nsec = 0 };
			while ((pselect(t.cmdfd + 1, &z, NULL, NULL, &tz, NULL) > 0) && !XPending(d) && (te - ts) < 36e6 && (x.Count - rd) > 0) {
				if (FD_ISSET(t.cmdfd, &z)) {
					errno = 0;
					r = read(t.cmdfd, x.Data + rd, x.Count - rd);
					/* printf("%d\n", r); */
					if (r > -1)  {
						rd += r;
						num_reads++;
						/* x.Data[r] = 0; // Can be out of bounds */
						/* printf("%s", x.Data); */
						/* x.Count = r; */
						/* CommitWrite(&t.buf, r); */
						/* ProcessInput(&t, x); */
						t.render = true;
						te = get_time();
						/* printf("count - rd: %d\n", x.Count - rd); */
					} else {
						break;
					}
				} else {
					break;
				}
			}

			/* if (num_reads) { */
			/* 	printf("num_reads: %ld\n", num_reads); */
			/* } */
				if (rd != 0) {
					x.Count = rd;
					/* printf("%.*s\n", rd, x.Data); */
					CommitWrite(&t.buf, rd);
					ProcessInput(&t, &c, x);
				}

			TracyCZoneEnd(ctx);
		}

		{
			TracyCZoneN(ctx, "drawing loop", true);
			if (t.render) {
				clear_screen(t.screen, t.tw, t.th);

				Render(&t);
				t.render = false;

				TracyCZoneN(ctx, "uploading to gpu", true);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, t.tw , t.th, GL_RGBA_INTEGER, GL_UNSIGNED_INT, t.screen);
				/* glFinish(); */
				TracyCZoneEnd(ctx);

				{
					TracyCZoneN(ctx, "Rendering", true);
					glDrawArrays(GL_TRIANGLES, 0, 3);
					glFinish();
					glXSwapBuffers(d, t.xw.win);
					/* XFlush(d); */
					TracyCZoneEnd(ctx);
				}
			}
			TracyCZoneEnd(ctx);
		}

		TracyCFrameMarkEnd("Frame");

	}

	/* clean_glcontext(ctx); */
	XCloseDisplay(d);
}

