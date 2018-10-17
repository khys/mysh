/*
 * main.c
 * syntax: "mysh"
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#define MAXARG  60
#define BUFSIZE 1024
#define MAXPIPE 10

#define TKN_NORMAL    0
#define TKN_REDIR_IN  1
#define TKN_REDIR_OUT 2
#define TKN_PIPE      3
#define TKN_BG        4
#define TKN_EOL       5
#define TKN_EOF       6

int getargs(int *, char *[], char *, char **, char **);
int gettoken(char *, int);

int main()
{
	int pid, ac[MAXPIPE], stat[MAXPIPE], tkn;
	int i, j, pnum;
	char *av[MAXPIPE][MAXARG], buf[MAXPIPE][BUFSIZE];
	char *redir_in[MAXPIPE], *redir_out[MAXPIPE];
	int pfd[MAXPIPE][2];
	int fd, fd1, fd2;

	struct sigaction sa_ignore;

	fd = open("/dev/tty", O_RDWR);

	for (;;) {
		for (i = 0; i < MAXPIPE; i++) {
			redir_in[i] = NULL;
			redir_out[i] = NULL;
		}
		
		fprintf(stderr, "** pgid: %d **\n", getpgrp());
		fprintf(stderr, "** pid:  %d **\n", getpid());
		fprintf(stderr, "mysh$ ");
		memset(av, 0, sizeof av);
		for (pnum = 0; pnum < MAXPIPE; pnum++) {
			tkn = getargs(&ac[pnum], av[pnum], buf[pnum],
						  &redir_in[pnum], &redir_out[pnum]);
			if (tkn == TKN_EOL || tkn == TKN_EOF) {
				break;
			}
		}
		if (!strcmp(av[0][0], "exit")) {
			// exit command
			exit(0);
		} else if (!strcmp(av[0][0], "cd")) {
			// cd command
			chdir(av[0][1]);
			continue;
		}

		fprintf(stderr, "** argv start **\n");
		for (j = 0; j < pnum + 1; j++) {
			for (i = 0; i < ac[j]; i++) {
				fprintf(stderr, "av[%d][%d]: %s\n", j, i, av[j][i]);
			}
		}
		fprintf(stderr, "** argv end **\n");

		// SIGTTOU		
		memset(&sa_ignore, 0, sizeof(sa_ignore));
		sa_ignore.sa_handler = SIG_IGN;
		if (sigaction(SIGTTOU, &sa_ignore, NULL) < 0) {
			perror("sigaction");
			exit(1);
		}

		if (pnum > 0) {
			pipe(pfd[0]);
		}
		if ((pid = fork()) < 0) {
			perror("fork");
			exit(1);
		}
		if (pid == 0) {
			// child process
			setpgid(getpid(), getpid());
			tcsetpgrp(fd, getpid());
			fprintf(stderr, "** pgid: %d **\n", getpgrp());
			fprintf(stderr, "** pid:  %d **\n", getpid());
			if (pnum > 0) {
				close(1);
				dup(pfd[0][1]);
				close(pfd[0][0]); close(pfd[0][1]);
			}

			// redirect
			if (redir_in[i] != NULL) {
				if ((fd1 = open(redir_in[i], O_RDONLY)) < 0) {
					perror("open");
					exit(1);
				}
				close(0);
				dup(fd1);
				close(fd1);
			}
			if (redir_out[i] != NULL) {
				if ((fd2 = open(redir_out[i],
								O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
					perror("open");
					exit(1);
				}
				close(1);
				dup(fd2);
				close(fd2);
			}
			
			if (execvp(av[0][0], av[0]) < 0) {
				perror("execvp");
				exit(1);
			}
		}

		// parent process
		fprintf(stderr, "** parent wait start **\n");
		if (wait(&stat[0]) < 0) {
			perror("wait");
			exit(1);
		}
		fprintf(stderr, "** parent wait end: %d **\n", stat[0]);
		
		// ctrl-C
		memset(&sa_ignore, 0, sizeof(sa_ignore));
		sa_ignore.sa_handler = SIG_IGN;
		if (sigaction(SIGINT, &sa_ignore, NULL) < 0) {
			perror("sigaction");
			exit(1);
		}
		
		for (i = 1; i <= pnum; i++) {
			if (i < pnum) {
				pipe(pfd[i]);
			}
			if ((pid = fork()) < 0) {
				perror("fork");
				exit(1);
			}
			if (pid == 0) {
				// child process
				setpgid(getpid(), tcgetpgrp(fd));
				fprintf(stderr, "** pgid: %d **\n", getpgrp());
				fprintf(stderr, "** pid:  %d **\n", getpid());
				
				close(0);
				dup(pfd[i - 1][0]);
				close(pfd[i - 1][0]); close(pfd[i - 1][1]);
				if (i < pnum) {
					close(1);
					dup(pfd[i][1]);
					close(pfd[i][0]); close(pfd[i][1]);
				}
				
				// redirect
				if (redir_in[i] != NULL) {
					if ((fd1 = open(redir_in[i], O_RDONLY)) < 0) {
						perror("open");
						exit(1);
					}
					close(0);
					dup(fd1);
					close(fd1);
				}
				if (redir_out[i] != NULL) {
					if ((fd2 = open(redir_out[i],
									O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
						perror("open");
						exit(1);
					}
					close(1);
					dup(fd2);
					close(fd2);
				}
				
				if (execvp(av[i][0], av[i]) < 0) {
					perror("execvp");
					exit(1);
				}
			}

			// parent process
			fprintf(stderr, "** parent wait start **\n");
			close(pfd[i - 1][0]); close(pfd[i - 1][1]);
			if (wait(&stat[i]) < 0) {
				perror("wait");
				exit(1);
			}
			fprintf(stderr, "** parent wait end: %d **\n", stat[i]);
		}

		// ctrl-C
		memset(&sa_ignore, 0, sizeof(sa_ignore));
		sa_ignore.sa_handler = SIG_DFL;
		if (sigaction(SIGINT, &sa_ignore, NULL) < 0) {
			perror("sigaction");
			exit(1);
		}

		tcsetpgrp(fd, getpgrp());

		// SIGTTOU		
		memset(&sa_ignore, 0, sizeof(sa_ignore));
		sa_ignore.sa_handler = SIG_DFL;
		if (sigaction(SIGTTOU, &sa_ignore, NULL) < 0) {
			perror("sigaction");
			exit(1);
		}
	}
	
	return 0;
}

int getargs(int *ac, char *av[], char *p, char **redir_in, char **redir_out)
{
	int tkn;
	*ac = 0;
	
	while ((tkn = gettoken(p, BUFSIZE)) == TKN_NORMAL) {
		av[(*ac)++] = p;
		p += strlen(p) + 1;
	}
	if (tkn == TKN_REDIR_IN) {
		p += strlen(p) + 1;
	    tkn = gettoken(p, BUFSIZE);
		*redir_in = p;
		p += strlen(p) + 1;
	} else if (tkn == TKN_REDIR_OUT) {
		p += strlen(p) + 1;
	    tkn = gettoken(p, BUFSIZE);
		*redir_out = p;
		p += strlen(p) + 1;
	}
	
	return tkn;
}

int gettoken(char *token, int len)
{
	char c;
	
	while (isblank(c = getc(stdin))) {}
	switch (c) {
	case '<':
		return TKN_REDIR_IN;
	case '>':
		return TKN_REDIR_OUT;
	case '|':
		return TKN_PIPE;
	case '&':
		return TKN_BG;
	case '\n':
		return TKN_EOL;
	case EOF:
		return TKN_EOF;
	default:
		*token++ = c;
		for (;;) {
			c = getc(stdin);
			if (isblank(c) || c == '<' || c == '>' || c == '|' ||
				c == '&' || c == '\n' || c == EOF) {
				break;
			}
			*token++ = c;
		}
		ungetc(c, stdin);
		*token = '\0';
		return TKN_NORMAL;
	}
}
