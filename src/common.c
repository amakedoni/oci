#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t interrupted = 0;

static void on_signal(int number)
{
    interrupted = number;
}

int setup_signals(void)
{
    struct sigaction action = {0};
    if (sigemptyset(&action.sa_mask) == -1) return -1;
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL) == -1) return -1;
    action.sa_handler = on_signal;
    if (sigaction(SIGINT, &action, NULL) == -1) return -1;
    return sigaction(SIGTERM, &action, NULL);
}

int read_byte(int fd, char *byte)
{
    ssize_t count;
    do { count = read(fd, byte, 1); }
    while (count == -1 && errno == EINTR && !interrupted);
    return (int)count;
}

int write_byte(int fd, char byte)
{
    ssize_t count;
    do { count = write(fd, &byte, 1); }
    while (count == -1 && errno == EINTR && !interrupted);
    if (count == 0) errno = EIO;
    return count == 1 ? 0 : -1;
}

int receive_reply(int fd)
{
    char reply;
    int count = read_byte(fd, &reply);
    if (count == 0) errno = EPIPE;
    return count == 1 ? (unsigned char)reply : -1;
}

int close_fd(int *fd)
{
    if (*fd < 0) return 0;
    int old = *fd;
    *fd = -1;
    if (close(old) == -1) {
        perror("close");
        return -1;
    }
    return 0;
}

int reap_child(pid_t pid)
{
    int status;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); }
    while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
    if (dprintf(STDERR_FILENO, "[parent pid=%ld] child=%ld status=%d; reaped\n",
                (long)getpid(), (long)pid, code) < 0) {
        perror("parent: log");
        return EXIT_FAILURE;
    }
    return code;
}
