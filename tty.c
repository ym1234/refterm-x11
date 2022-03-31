/* static int cmdfd; */

/* void */
/* sigchld(int a) */
/* { */
/* 	int stat; */
/* 	pid_t p; */

/* 	if ((p = waitpid(pid, &stat, WNOHANG)) < 0) */
/* 		die("waiting for pid %hd failed: %s\n", pid, strerror(errno)); */

/* 	if (pid != p) */
/* 		return; */

/* 	if (WIFEXITED(stat) && WEXITSTATUS(stat)) */
/* 		die("child exited with status %d\n", WEXITSTATUS(stat)); */
/* 	else if (WIFSIGNALED(stat)) */
/* 		die("child terminated due to signal %d\n", WTERMSIG(stat)); */
/* 	/1* _exit(0); *1/ */
/* } */

#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)

// Taken from st
void
execsh(char *cmd, char **args)
{
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

	/* printf("%s, %s\n", prog, args[0]); */
	execvp(prog, args);
	_exit(1);
}

int ttynew(char *cmd, char **args) {
	int m, s;

	if (openpty(&m, &s, NULL, NULL, NULL) < 0)
		die("openpty failed: %s\n", strerror(errno));

	int pid;
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
		execsh(cmd, args);
		break;
	default:
		close(s);
		/* cmdfd = m; */
		/* signal(SIGCHLD, sigchld); */
		signal(SIGCHLD, SIG_DFL);
		break;
	}
	return m;
}
