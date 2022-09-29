#include <X11/Xlib.h>
#include <glad/glad.h>
#include <GL/glx.h>
/* #include <GL/glu.h> */

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void* userParam) {
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
			(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

char *read_file(char *name) {
	FILE* fd = fopen(name, "r");
	fseek(fd, 0L, SEEK_END);
	int sz = ftell(fd);
	fseek(fd, 0, SEEK_SET);
	char *buf = malloc(sz + 1);
	fread(buf, sizeof(char), sz, fd);
	buf[sz] = 0;
	return buf;
}

GLuint compile_shader(char * name, int shadertype) {
	GLuint shader = glCreateShader(shadertype);

	char *buf = read_file(name);
	glShaderSource(shader, 1, (const char * const*) &buf, NULL);
	glCompileShader(shader);
	free(buf);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		die("Couldn't compile shader");
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
	if (!success) {
		die("Link failure\n");
	}
	return prog;
}

GLXFBConfig getfbc(Display *d) {
	static Display *dp = NULL;
	static GLXFBConfig best_fbc; // defined as a pointer in GL/glx.h but probably shouldn't really depend on that (it's opaque for a reason)
	if (d == dp && best_fbc) {
		return best_fbc;
	}

	dp = d;
	static int visual_attribs[] = {
		GLX_X_RENDERABLE     , True,
		GLX_DRAWABLE_TYPE    , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE      , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE    , GLX_TRUE_COLOR,
		GLX_RED_SIZE         , 8,
		GLX_GREEN_SIZE       , 8,
		GLX_BLUE_SIZE        , 8,
		GLX_ALPHA_SIZE       , 8,
		GLX_DEPTH_SIZE       , 0,
		GLX_STENCIL_SIZE     , 0,
		GLX_DOUBLEBUFFER     , True,
		/* GLX_SAMPLE_BUFFERS, 1, */
		/* GLX_SAMPLES       , 4, */
		None
	};

	int fbcount;
	GLXFBConfig* fbc = glXChooseFBConfig(d, DefaultScreen(d), visual_attribs, &fbcount);
	if (!fbc) die("Failed to retrieve a framebuffer config\n");

	int best_num_samp = -1;
	for (int i = 0; i < fbcount; ++i) {
		int samp_buf, samples;
		glXGetFBConfigAttrib(d, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
		glXGetFBConfigAttrib(d, fbc[i], GLX_SAMPLES       , &samples );
		if (samp_buf && samples > best_num_samp) best_fbc = fbc[i], best_num_samp = samples;
	}
	return best_fbc;
}

GLXContext create_glcontext(Display *d) {
	GLXFBConfig fbc = getfbc(d);
	glXCreateContextAttribsARBProc glXCreateContextAttribsARB =
		(glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");

	static int context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
	};
	GLXContext ctx = glXCreateContextAttribsARB(d, fbc, 0, True, context_attribs);
	if (!ctx) die("ctx creation failed");
	return ctx;
}
