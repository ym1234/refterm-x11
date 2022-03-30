#include <stdio.h>
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
#include <pwd.h>
#include <poll.h>

void die(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    exit(-1);
}

#include "freetype.c"
#include "tty.c"
#include "ogl.c"

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

char *render(int columns, int rows, unsigned int *screen, char* start, char *b) {
	for (int i = rows - 1; i >= 0; --i) {
		for (int j = columns - 1; j >= 0; --j) {
			/* char c = *(b++); */
			/* if () { */
			/* 	--b; */
			/* 	goto out; */
			/* } else if (c == '\n') { */
			char c = *(b--);
			if (b == start) {
				goto out;
			}
			if (c == '\n') {
				j = -1;
				continue;
			}
			int z = 4 * ((i * columns) + j);
			screen[z] = (c - 32) % 10;
			screen[z + 1] = (c - 32) / 10;
		}
	}
out:
	return b;
}


void prep_tex(unsigned int tex, int texnum) {
    glActiveTexture(GL_TEXTURE0 + texnum);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    setlocale(LC_ALL, "C.UTF-8");
	assert(argc > 1);


    Display *d = XOpenDisplay(NULL);
    if (d == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(-1);
    }
    Window win = create_window(d);

    FT_Library library;
    if(FT_Init_FreeType(&library)) {
        die("Couldn't load freetype");
    }
    FTFont font = load_font(library, "/home/ym/.local/share/fonts/curieMedium.otb", 0, 12);
    /* FTFont font = load_font(library, "/usr/share/fonts/misc/ter-u28b.otb", 0, 16); */
    /* FTFont font = load_font(library, "/usr/share/fonts/liberation/LiberationMono-Regular.ttf", 0, 16); */
    /* FTFont font = load_font(library, "/usr/share/fonts/TTF/Roboto-Regular.ttf", 0, 16); */
    /* FTFont font = load_font(library, "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", 0, 16); */

    int shaders[2] = { compile_shader("grid.v.glsl", GL_VERTEX_SHADER), compile_shader("grid.f.glsl", GL_FRAGMENT_SHADER) };
    unsigned int prog = create_program(2, shaders);
    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);

    /* float vertices[] = { -1., 1., -1., -1., 1., 1., 1., -1.}; */
    /* float vertices[] = { -1., -1., 3., -1., -1., 3.}; */
    float vertices[] = { -1., 3, -1., -1., 3., -1 };

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

    glUniform2ui(3, font.cellwidth, font.cellheight); // Cell Size
    glUniform2ui(5, 4, 4); // TopLeftMargin
    /* glUniform2ui(5, 0, 0); // TopLeftMargin */

    glUniform1ui(6, 0xffffffff); // BlinkModulate
    glUniform1ui(7, 0x1d1f21); // MarginColor
    glClearColor(0x1d, 0x1f, 0x21, 0xff);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2ui(8, 16/2 - 16/10, 16/2 + 16/10); // StrikeThrough
    glUniform2ui(9, font.cellheight - font.descent + font.underline - (int)(font.underline_thickness  / 2.0 + 0.5),  font.cellheight - font.descent + font.underline - (int)(font.underline_thickness  / 2.0 + 0.5) + font.underline_thickness); // underline
    /* glUniform2ui(9, 10, 2); */


    unsigned int tex[2];
    glGenTextures(2, tex);

    glUniform1i(1, 0);
    prep_tex(tex[0], 0);

    float *data = render_ascii(font, library);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, font.cellwidth * 10, font.cellheight * 10, 0, GL_RGBA, GL_FLOAT, data);

    glUniform1i(2, 1);
    prep_tex(tex[1], 1);

    glFinish();


    XWindowAttributes gwa;
    XGetWindowAttributes(d, win, &gwa);

    int columns = (gwa.width - 4) / font.cellwidth;
    int rows = (gwa.height - 4) / font.cellheight;
    unsigned int *my_vertices = update_screen(columns, rows);

    char framerate_c[1000] = {};
    XEvent xev;

    struct timeval time;
    double frametime = 0;
    int framerate = 1;

    int width = gwa.width, height = gwa.height;

	int cmdfd = ttynew("/bin/sh", argv+1);

	fd_set rfd;
	FD_ZERO(&rfd);
	FD_SET(cmdfd, &rfd);

	int flags = fcntl(cmdfd, F_GETFL, 0);
	fcntl(cmdfd, F_SETFL, flags | O_NONBLOCK);

#define BUFFER_SIZE 0xFFFFFFFF
    char *screen = calloc(BUFFER_SIZE, sizeof(char));
	screen[0] = '~' + 1;
	int pos = 0;
	int scroll = 0;
    size_t num_frames = 1;
    double frametime_total = 0;
    size_t sum_fps = 0;


    while(1) {
        gettimeofday(&time, NULL);
        long start_time = 1000000 * time.tv_sec + time.tv_usec;

        int pending = XPending(d);
        while (pending) {
            XNextEvent(d, &xev);
			if (xev.type == ButtonPress){
				switch (xev.xbutton.button){
					case Button4:
						if(scroll) {
							--scroll;
						}
						while (scroll && screen[--scroll] != '\n');
						if (scroll) {
							++scroll;
						}
						/* printf("up"); */
						break;
					case Button5:
						while (scroll < pos && screen[scroll++] != '\n') {
						}
						/* printf("down"); */
						break;
					default:
				}
			}
            if (xev.type == ConfigureNotify) {
                XGetWindowAttributes(d, win, &gwa);
                if (gwa.width == width && gwa.height == height) {
                    pending--;
                    continue;
                }

                columns = (gwa.width - 4) / font.cellwidth;
                rows = (gwa.height - 4) / font.cellheight;
                printf("columns: %d, rows: %d\n", columns, rows);
                free(my_vertices);

                my_vertices = update_screen(columns, rows);
                width = gwa.width, height = gwa.height;

                glScissor(0, 0, gwa.width, gwa.height);
                glEnable(GL_SCISSOR_TEST);
                glViewport(0, 0, gwa.width, gwa.height);
            } else if (xev.type = KeyPress) {
                char b[36] = {};
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
                screen[pos] = '~' + 1;
                screen[pos + 1] = '\0';
            }
            pending--;
        }

		// TODO(ym):
		/* struct timeval tv = {}; */
		/* tv.tv_sec = 10; */
		/* tv.tv_usec = 0; */
		/* int r = select(cmdfd+1, &rfd, NULL, NULL, &tv); */
		/* printf("%d\n", r); */
		/* if (r < 0) { */
		/* 	if (errno == EINTR) */
		/* 		continue; */
		/* 	die("select failed: %s\n", strerror(errno)); */
		/* } else if (r > 0) { */
		/* 	int k = read(cmdfd, screen + pos, r); */
		/* 	printf("Read %d, %d, %s, %s\n", r, k, strerror(errno), screen + pos); */
		/* 	if (k > 0) { */
		/* 		screen[pos + k] = '~' + 1; */
		/* 		pos += k; */
		/* 	} */
		/* } */
		/* printf("%d\n", r); */
		/* printf("Here\n"); */

		// TODO(ym):
		/* int n = 0; */
		/* if ((n = poll(&cmdfd, 1, -1)) >= 0) { */
		/* 	/1* if (!((screen + pos + n) >= BUFFER_SIZE)) { *1/ */
		/* 	printf("%s\n", strerror(errno)); */
		/* 	errno = 0; */
		/* 		int k = read(cmdfd, screen + pos, n); */
		/* 		printf("Read %d, %d, %s, %s\n", n, k, strerror(errno), screen + pos); */
		/* 		if (k > 0) { */
		/* 			screen[pos + k] = '~' + 1; */
		/* 			pos += k; */
		/* 		} */
		/* 	/1* } *1/ */
		/* } */
		/* errno = 0; */


		errno = 0;
		if (pos < BUFFER_SIZE) {
			int k = read(cmdfd, screen + pos, BUFFER_SIZE - pos);
			if (k > 0) {
				screen[pos + k] = '\n';
				screen[pos + k + 1] = '~' + 1;
				pos += k + 1;
				/* printf("Read %d, %d, %s, %s\n", 0, k, strerror(errno), screen + pos - k); */
				/* fflush(stdout); */
			}
		}

        clear_screen(my_vertices, columns, rows);
        char *c = render(columns, rows, my_vertices, screen, screen - scroll + pos);

        snprintf(framerate_c, 1000, "FPS: %5d, mSPF: %f, aFPS: %f, amSPF %f, columns: %d, rows: %d", framerate, frametime * 100, (double) sum_fps / num_frames, (double) frametime_total / num_frames, columns, rows);
        size_t framelen = strlen(framerate_c);

        for (int i = 0; i < framelen; i++) {
            int idx = 4 * (columns * rows - framelen + i);
            char c = framerate_c[i] - 32;
            my_vertices[idx] = c % 10;
            my_vertices[idx + 1] = c / 10;
        }

        /* glTexSubImage2D(GL_TEXTURE_2D, 0, columns - framelen, rows - 1, framelen, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices + (columns * rows * 4 - 4 * framelen)); */
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, columns, rows, GL_RGBA_INTEGER, GL_UNSIGNED_INT, my_vertices);

        {
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glXSwapBuffers(d, win);
            /* glFinish(); */
        }

        gettimeofday(&time, NULL);
        long end_time = 1000000 * time.tv_sec + time.tv_usec;

        frametime = ((double)(end_time - start_time) / 1000000);
        framerate = (int) (1 / frametime);

        sum_fps += framerate;
        frametime_total += frametime;
        ++num_frames;
		/* usleep(1000000); */
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(prog);

    XCloseDisplay(d);

    return 0;
}
