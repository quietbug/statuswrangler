#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define MAX_PIPES 128
#define MAX_BUF 1024
#define REOPEN_INTERVAL_SEC 0.1f

static int num_pipes = 0;
static char pipe_paths[MAX_PIPES][256];
static int pipe_fds[MAX_PIPES];
static char pipe_buffers[MAX_PIPES][MAX_BUF];
static char base_path[256] = ".";
static int layout_vertical = 1;
static int strip_spaces = 0;
static volatile sig_atomic_t running = 1;

static void cleanup(void) {
    for (int i = 0; i < num_pipes; i++) {
        if (pipe_fds[i] >= 0)
            close(pipe_fds[i]);
        unlink(pipe_paths[i]);
    }
}

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void trim(char *s) {
    if (!strip_spaces) return;
    char *end, *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (*start == 0) { *s = 0; return; }
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    if (start != s) memmove(s, start, end + 2 - start);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "shvn:p:")) != -1) {
        switch (opt) {
            case 's': strip_spaces = 1; break;
            case 'h': layout_vertical = 0; break;
            case 'v': layout_vertical = 1; break;
            case 'n': num_pipes = atoi(optarg); break;
            case 'p': strncpy(base_path, optarg, sizeof(base_path)-1); break;
            default:
                fprintf(stderr, "Usage: %s [-s] [-h|-v] -n <n> -p <path>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (num_pipes <= 0 || num_pipes > MAX_PIPES) {
        fprintf(stderr, "Invalid number of pipes.\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_sigint);
    atexit(cleanup);

    // create and open pipes
	for (int i = 0; i < num_pipes; i++) {
		size_t base_len = strnlen(base_path, sizeof(base_path) - 16);
		snprintf(pipe_paths[i], sizeof(pipe_paths[i]), "%.*s/pipe%d",
				 (int)base_len, base_path, i + 1);
		unlink(pipe_paths[i]);
		if (mkfifo(pipe_paths[i], 0666) == -1) {
			perror("mkfifo");
			exit(EXIT_FAILURE);
		}
		pipe_fds[i] = open(pipe_paths[i], O_RDONLY | O_NONBLOCK);
		if (pipe_fds[i] == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}
		memset(pipe_buffers[i], 0, sizeof(pipe_buffers[i]));
	}

    struct pollfd pfds[MAX_PIPES];
    for (int i = 0; i < num_pipes; i++) {
        pfds[i].fd = pipe_fds[i];
        pfds[i].events = POLLIN;
    }

	while (running) {
		int ready = poll(pfds, num_pipes, -1);
		if (ready < 0) {
			if (errno == EINTR) continue;
			perror("poll");
			break;
		}

		int got_data = 0;

		for (int i = 0; i < num_pipes; i++) {
			if (pfds[i].fd == -1) continue;

			if (pfds[i].revents & POLLIN) {
				char tmp[MAX_BUF];
				ssize_t bytes = read(pipe_fds[i], tmp, sizeof(tmp) - 1);
				if (bytes > 0) {
					tmp[bytes] = '\0';
					strncpy(pipe_buffers[i], tmp, sizeof(pipe_buffers[i]) - 1);
					trim(pipe_buffers[i]);
					got_data = 1;
				} else if (bytes == 0) {
					// writer closed
					close(pipe_fds[i]);
					pipe_fds[i] = -1;
					pfds[i].fd = -1;
				}
			}

			if (pfds[i].revents & POLLHUP) {
				// FIFO has no writers, but we’ll try reopening lazily
				if (pipe_fds[i] != -1) {
					close(pipe_fds[i]);
					pipe_fds[i] = -1;
					pfds[i].fd = -1;
				}
			}

			pfds[i].revents = 0;
		}

		// try reopening any closed pipes once per loop iteration
		for (int i = 0; i < num_pipes; i++) {
			if (pipe_fds[i] == -1) {
				int fd = open(pipe_paths[i], O_RDONLY | O_NONBLOCK);
				if (fd >= 0) {
					pipe_fds[i] = fd;
					pfds[i].fd = fd;
					pfds[i].events = POLLIN;
				}
			}
		}

		if (got_data) {
			if (layout_vertical) {
				for (int i = 0; i < num_pipes; i++)
					printf("%d) %s\n", i + 1, pipe_buffers[i]);
			} else {
				for (int i = 0; i < num_pipes; i++) {
					printf("%s", pipe_buffers[i]);
					if (i < num_pipes - 1) printf(" | ");
				}
				printf("\n");
			}
			fflush(stdout);
		}
		usleep((useconds_t)((REOPEN_INTERVAL_SEC) * 1000000.0));
	}

    return 0;
}
