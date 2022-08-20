// idk if i want this

#if 0
static void (*handler[LASTEvent])(XEvent *) = {
	[KeyPress] = kpress,
	[ClientMessage] = cmessage,
	[ConfigureNotify] = resize,
	[VisibilityNotify] = visibility,
	[UnmapNotify] = unmap,
	[Expose] = expose,
	[FocusIn] = focus,
	[FocusOut] = focus,
	[MotionNotify] = bmotion,
	[ButtonPress] = bpress,
	[ButtonRelease] = brelease,
/*
 * Uncomment if you want the selection to disappear when you select something
 * different in another window.
 */
/*	[SelectionClear] = selclear_, */
	[SelectionNotify] = selnotify,
/*
 * PropertyNotify is only turned on when there is some INCR transfer happening
 * for the selection retrieval.
 */
	[PropertyNotify] = propnotify,
	[SelectionRequest] = selrequest,
};
#endif
typedef struct {
	size_t start;
	size_t length;

	uint32_t fg;
	uint32_t bg;
	int properties;
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
	Display *d;
	Window win;

	GLXContext ctx;
	int w, h;
	XSelection xsel;
	Atom wmdeletewin, netwmpid, netwmiconname, netwmname;
} XWindow;

typedef struct {
	int VBO, VAO;
	int vertices[6];

	unsigned int tex[2];
	unsigned int prog;
} OGLContext;

typedef struct {
	XWindow xw;

	cbuf buf;
	Segment *sg;
	int cmdfd;
	int borderpx;

	int tw, th; // tty size
	int gw, gh; // Glyph dimensions
	Font pf; // primary font
	Font fbf; // fallback font
} Terminal;


void set_terminalsz(Terminal *t);

void resize(Terminal *t, XEvent *e) {
	t->xw.w = e->xconfigure.width;
	t->xw.h = e->xconfigure.height;
	set_terminalsz(t);
}

static void (*handler[LASTEvent])(Terminal *, XEvent *) = {
	[KeyPress] = NULL,
	[ConfigureNotify] = resize,
	[ButtonPress] = NULL,
	[VisibilityNotify] = NULL,
	[UnmapNotify] = NULL,
	[ButtonRelease] = NULL,
};


void
xsettitle(XWindow *xw, char *p)
{
	XTextProperty prop;
	p = p ? p : "Refterm";

	if (Xutf8TextListToTextProperty(xw->d, &p, 1, XUTF8StringStyle,
	                                &prop) != Success)
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

	win->ctx = create_glcontext(d, fbc);
}

void map_window(XWindow *xw) {
	glXMakeCurrent(xw->d, xw->win, xw->ctx);
	XMapWindow(xw->d, xw->win);

	{
		XEvent xev;
		do {
			XNextEvent(xw->d, &xev);
			if (XFilterEvent(&xev, None)) {
				continue;
			} else if (xev.type == ConfigureNotify) {
				xw->w = xev.xconfigure.width;
				xw->h = xev.xconfigure.height;
			}
		} while (xev.type != MapNotify);
	}
}

void set_terminalsz(Terminal *t) {
	if (t->gh == 0 || t->gw == 0) {
		return;
	}
	t->th = (t->xw.h - t->borderpx) / t->gh;
	t->tw = (t->xw.w - t->borderpx) / t->gw;
}

Terminal create_terminal(Display *d, ssize_t bz, char **args) {
	Terminal t = {};
	t.cmdfd = ttynew("/bin/bash", args);

	create_xwindow(d, &t.xw);
	t.buf = create_cbuf(bz);
	/* t.tz = terminalsize(wz, f); */
	return t;
}
