// idk if i want this

typedef struct {
	uint32_t fg;
	uint32_t bg;
	int properties;
} CellProperties;

typedef struct {
	source_buffer_range buf;
	CellProperties props;
	bool line_end;
	/* hb_glyph_info_t *hb_info; */
	/* hb_glyph_position_t *hb_pos; */
} Segment;

typedef struct {
	Atom xtarget;
	char *primary, *clipboard;
	struct timespec tclick1;
	struct timespec tclick2;
} XSelection;

typedef struct {
	int x, y;
	CellProperties props;
} TCursor;

typedef struct {
	GLXContext ctx;

	unsigned int VAO, VBO;
	int vertices[6];

	unsigned int program;

	unsigned int glyph_tex;
	unsigned int cell_tex;

	unsigned int gt_slot;
	unsigned int ct_slot;
} GLResources;

typedef struct {
	Display *d;
	Window win;

	int w, h;
	XSelection xsel;
	Atom wmdeletewin, netwmpid, netwmiconname, netwmname;
} XWindow;

static size_t MAX_SEGMENTS =  10485760;//  0x600000;
/* #define MAX_SEGMENTS 0x6000 */
typedef struct {
	source_buffer buf;

	Segment *sg;

	size_t SegmentCount;

	int cmdfd;
	bool render;

	int borderpx;

	int tw, th; // tty size
	int gw, gh; // Glyph dimensions

	unsigned int *screen; // always synced with tty size
	/* size_t *offsets; */
	/* size_t *lines; */
	/* size_t CurrentLine; */
	int CursorX, CursorY;
	/* size_t AnchorLine; */
	/* size_t CurrentLineWidth; */

	Font pf; // primary font
	Font fbf; // fallback font

	XWindow xw;
	GLXContext glctx;
	GLResources glres;
} Terminal;

void xsettitle(XWindow *xw, char *p) {
	XTextProperty prop;
	p = p ? p : "Refterm";

	if (Xutf8TextListToTextProperty(xw->d, &p, 1, XUTF8StringStyle, &prop) != Success)
		return;
	XSetWMName(xw->d, xw->win, &prop);
	XSetTextProperty(xw->d, xw->win, &prop, xw->netwmname);
	XFree(prop.value);
}

#define DIM_X 1280
#define DIM_Y 720
void create_xwindow(Display *d, XWindow *win) {
	GLXFBConfig fbc = getfbc(d);
	XVisualInfo *vi = glXGetVisualFromFBConfig(d, fbc);
	win->d = d;
	XSetWindowAttributes swa = {
		.colormap = XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone),
		.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask,
		.background_pixmap = None,
		.border_pixel = 0
	};

	win->win =
		XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, DIM_X, DIM_Y, 0, vi->depth,
				InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
	XSelectInput(d, win->win, KeyPressMask | ExposureMask | StructureNotifyMask | PropertyChangeMask	| ButtonPressMask);
	XFree(vi);
	if(!win->win) die("window");

	win->wmdeletewin = XInternAtom(d, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(d, win->win, &win->wmdeletewin, 1);

	win->netwmpid = XInternAtom(d, "_NET_WM_PID", False);
	pid_t pid = getpid();
	XChangeProperty(d, win->win, win->netwmpid, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);

	win->netwmname = XInternAtom(d, "_NET_WM_NAME", False);
	win->netwmname = XInternAtom(d, "_NET_WM_ICON_NAME", False);
	xsettitle(win, "Refterm");

	/* win->xsel.primary = NULL; */
	/* win->xsel.clipboard = NULL; */
	/* win->xsel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0); */
	/* if (xsel.xtarget == None) */
	/* 	xsel.xtarget = XA_STRING; // idk what this is */

}
void resize(Terminal *t, XEvent *e);
void map_window(Terminal *t) {
	assert(glXMakeCurrent(t->xw.d, t->xw.win, t->glctx) == True);
	XMapWindow(t->xw.d, t->xw.win);

	{
		TracyCZoneN(ctx, "map window resize", 4);
		XEvent xev;
		do {
			XNextEvent(t->xw.d, &xev);
			if (XFilterEvent(&xev, None)) {
				continue;
			} else if (xev.type == ConfigureNotify) {
				resize(t, &xev);
			}
		} while (xev.type != MapNotify);
		TracyCZoneEnd(ctx);
	}
}


void resize_screen(Terminal *t) {

/* 	static int startup = 0; */

/* 	size_t *lines_tmp = calloc(t->th, sizeof(size_t)); */
/* 	size_t *offsets_tmp = calloc(t->th, sizeof(size_t)); */

/* 	if (startup) { */
/* 		for (int i = MIN(t->CurrentLine - t->AnchorLine, t->th) - 1; i >= 0 ; --i) { */
/* 			int idx = t->CurrentLine-- % oh; */
/* 			if (idx < 0) idx += oh; */
/* 			printf("idx: %d\n", idx); */
/* 			lines_tmp[i] = t->lines[idx]; */
/* 			offsets_tmp[i] = t->offsets[idx]; */
/* 		} */
/* 		/1* for (int i = 0; i < t->th; ++i) { *1/ */
/* 		/1* 	t->lines[i] = (size_t) -1; *1/ */
/* 		/1* } *1/ */
/* 		/1* t->offsets = calloc(t->th, sizeof(size_t)); // TODO(ym): realloc this *1/ */
/* 	} */

	if(t->screen) free(t->screen);
	/* if(t->lines) free(t->lines); */
	/* if(t->offsets) free(t->offsets); */

	t->screen = calloc(4 * t->th * t->tw, sizeof(*t->screen));
	/* t->AnchorLine = 0; */
	/* t->lines = lines_tmp; //calloc(t->th, sizeof(size_t)); // TODO(ym): realloc this */
	/* t->offsets = offsets_tmp; //calloc(t->th, sizeof(size_t)); // TODO(ym): realloc this */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, t->tw, t->th, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,  (unsigned int *) NULL);// t->screen);
	/* startup = 1; */
}

/* void resize_screen(Terminal *t) { */
/* 	if(t->screen) {free(t->screen);} */
/* 	if(t->lines) {free(t->lines);} */
/* 	if(t->offsets) {free(t->offsets);} */

/* 	t->screen = calloc(4 * t->th * t->tw, sizeof(*t->screen)); */
/* 	t->lines = calloc(t->th, sizeof(size_t)); // TODO(ym): realloc this */
/* 	for (int i = 0; i < t->th; ++i) { */
/* 		t->lines[i] = (size_t) -1; */
/* 	} */
/* 	t->offsets = calloc(t->th, sizeof(size_t)); // TODO(ym): realloc this */

/* 	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, t->tw, t->th, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,  (unsigned int *) NULL);// t->screen); */
/* } */

void set_terminalsz(Terminal *t) {
	assert(t->gh != 0 && t->gw != 0);

	t->th = (t->xw.h - t->borderpx) / t->gh;
	t->tw = (t->xw.w - t->borderpx) / t->gw;
	glUniform2ui(4, t->tw, t->th);
	resize_screen(t);
}

Terminal create_terminal(Display *d, ssize_t bz, char **args) {
	Terminal t = {};
	/* t.cmdfd = ttynew("/bin/bash", args); */

	t.render = true;
	t.borderpx = 4;
	create_xwindow(d, &t.xw);
	t.buf = AllocateSourceBuffer(bz);
	t.sg = mmap(NULL, sizeof(Segment) * MAX_SEGMENTS, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	printf("Segments array size: %p, %ld\n", t.sg, sizeof(Segment) * MAX_SEGMENTS);
	assert(t.sg != MAP_FAILED);

	t.glctx = create_glcontext(d);
	/* t.CurrentLine = 0; */
	assert(glXMakeCurrent(t.xw.d, t.xw.win, t.glctx) == True);
	return t;
}

void resize(Terminal *t, XEvent *e) {
	if (e->xconfigure.width == t->xw.w && e->xconfigure.height == t->xw.h) {
		return;
	}

	t->render = true;
	t->xw.w = e->xconfigure.width;
	t->xw.h = e->xconfigure.height;
	set_terminalsz(t);

	// TODO(ym): Check if we are using the correct opengl context
	glScissor(0, 0, t->xw.w, t->xw.h);
	glViewport(0, 0, t->xw.w, t->xw.h);
}

// TODO(ym): IME input
void keypress(Terminal *t, XEvent *e) {
	char buf[128];
	KeySym ksym;
	int len = XLookupString(&e->xkey, buf, sizeof buf, &ksym, NULL);
	write(t->cmdfd, buf, len);
}

void cmessage(Terminal *t, XEvent *e) {
	if (e->xclient.data.l[0] == t->xw.wmdeletewin) {
		printf("Got wmdeletewin, exiting\n"); // TODO(ym): make this a flag or something
		exit(0);
	}
}

static void (*handler[LASTEvent])(Terminal *, XEvent *) = {
	[KeyPress] = keypress,
	[ClientMessage] = cmessage,
	[ConfigureNotify] = resize,
	[ButtonPress] = NULL,
	[VisibilityNotify] = NULL,
	[UnmapNotify] = NULL,
	[ButtonRelease] = NULL,
};
