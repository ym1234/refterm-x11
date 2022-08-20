#include <sys/wait.h>
static pid_t pid;

// Taken from st
void
sigchld(int a)
{
	int stat;
	pid_t p;

	if ((p = waitpid(pid, &stat, WNOHANG)) < 0)
		die("waiting for pid %hd failed: %s\n", pid, strerror(errno));

	if (pid != p)
		return;

	// If the process exists too early, we may end up in a situation where we can't exit because the signal happened when we were running a function that shouldn't be interrupted, which why we use _exit not exit is used in die
	// Should find a better solution for this, maybe just set a flag?
	if (WIFEXITED(stat) && WEXITSTATUS(stat))
		die("child exited with status %d\n", WEXITSTATUS(stat));
	else if (WIFSIGNALED(stat))
		die("child terminated due to signal %d\n", WTERMSIG(stat));
	_exit(0);
}

#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)

void execsh(char *cmd, char **args) {
	setbuf(stdout, NULL);
	char *sh, *prog, *arg;
	const struct passwd *pw;

	errno = 0;
	if ((pw = getpwuid(getuid())) == NULL) {
		if (errno)
			die("getpwuid: %s\n", strerror(errno));
		else
			die("who are you?\n");
	}

	if ((sh = getenv("SHELL")) == NULL)
		sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

	if (args) {
		prog = args[0];
		arg = NULL;
	} else {
		prog = sh;
		arg = NULL;
	}

	DEFAULT(args, ((char *[]) {prog, arg, NULL}));

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", sh, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("TERM", "HELLO", 1);

	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	printf("prog: %s, args:", prog);
	for (int i = 0; args[i] != NULL; i++) {
		printf("%s\n", args[i]);
	}
	execvp(prog, args);
	_exit(1);
}

static struct termios orig_termios;

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr"); }

void enableRawMode() {
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	/* raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); */
	raw.c_lflag &= ~(ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int ttynew(char *cmd, char **args) {
	int m, s;

	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));

	/* if(fcntl(m, F_SETPIPE_SZ, 65536) < 0) die("fork failed: %s\n", strerror(errno)); */
	/* if(fcntl(s, F_SETPIPE_SZ, 65536) < 0) die("x\n"); */

	switch (pid = fork()) {
	case -1:
		die("fork failed: %s\n", strerror(errno));
		break;
	case 0:
		setsid(); /* create a new process group */
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if (ioctl(s, TIOCSCTTY, NULL) < 0)
			die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));

		close(s);
		close(m);
		/* enableRawMode(); */
		execsh(cmd, args);
		break;
	default:
		close(s);
		signal(SIGCHLD, sigchld);
		/* signal(SIGCHLD, SIG_DFL); */
		break;
	}
	return m;
}
