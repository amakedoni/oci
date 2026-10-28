#include "common.h"

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t interrupted = 0;

static void on_signal(int signal_number)
{
    interrupted = signal_number;
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

int write_all(int fd, const void *data, size_t size)
{
    const unsigned char *position = data;
    while (size != 0) {
        ssize_t count = write(fd, position, size);
        if (count == -1 && errno == EINTR && !interrupted) continue;
        if (count <= 0) {
            if (count == 0) errno = EIO;
            return -1;
        }
        position += count;
        size -= (size_t)count;
    }
    return 0;
}

int log_message(const char *role, const char *format, ...)
{
    char message[768];
    int prefix = snprintf(message, sizeof message, "[%s pid=%ld] ", role, (long)getpid());
    if (prefix < 0 || (size_t)prefix >= sizeof message) return -1;
    va_list args;
    va_start(args, format);
    int count = vsnprintf(message + prefix, sizeof message - (size_t)prefix, format, args);
    va_end(args);
    if (count < 0) return -1;
    size_t size = (size_t)prefix + (size_t)count;
    if (size >= sizeof message - 1) size = sizeof message - 2;
    message[size++] = '\n';
    return write_all(STDERR_FILENO, message, size);
}

int close_fd(int *fd)
{
    if (*fd < 0) return 0;
    int old = *fd;
    *fd = -1;
    if (close(old) == -1) { perror("close"); return -1; }
    return 0;
}

int redirect_fd(int source, int target)
{
    int result;
    do { result = dup2(source, target); } while (result == -1 && errno == EINTR && !interrupted);
    return result == -1 ? -1 : 0;
}

int receive_reply(int fd, unsigned char *reply)
{
    ssize_t count;
    do { count = read(fd, reply, 1); } while (count == -1 && errno == EINTR && !interrupted);
    if (count == 0) errno = EPIPE;
    return count == 1 ? 0 : -1;
}

int read_line(struct line_reader *reader, int watch_fd, char **line, size_t *size)
{
    size_t capacity = 256;
    *size = 0;
    *line = malloc(capacity);
    if (*line == NULL) return -1;
    for (;;) {
        if (interrupted) { errno = EINTR; goto fail; }
        if (reader->begin == reader->end) {
            struct pollfd fds[2] = {
                {reader->fd, POLLIN, 0}, {watch_fd, POLLIN, 0}
            };
            int ready;
            do { ready = poll(fds, 2, -1); } while (ready == -1 && errno == EINTR && !interrupted);
            if (ready == -1) goto fail;
            if (watch_fd >= 0 && fds[1].revents != 0) {
                free(*line); *line = NULL; return -2;
            }
            ssize_t count;
            do { count = read(reader->fd, reader->buffer, sizeof reader->buffer); }
            while (count == -1 && errno == EINTR && !interrupted);
            if (count == -1) goto fail;
            if (count == 0) {
                (*line)[*size] = '\0';
                return *size != 0;
            }
            reader->begin = 0;
            reader->end = (size_t)count;
        }
        unsigned char byte = reader->buffer[reader->begin++];
        if (*size + 1 == capacity) {
            if (capacity > SIZE_MAX / 2) { errno = ENOMEM; goto fail; }
            size_t next = capacity * 2;
            char *grown = realloc(*line, next);
            if (grown == NULL) goto fail;
            *line = grown; capacity = next;
        }
        (*line)[(*size)++] = (char)byte;
        (*line)[*size] = '\0';
        if (byte == '\n') return 1;
    }
fail:
    free(*line); *line = NULL;
    return -1;
}

int reap_child(pid_t pid)
{
    int status;
    pid_t result;
    do { result = waitpid(pid, &status, 0); } while (result == -1 && errno == EINTR);
    if (result == -1) { perror("waitpid"); return EXIT_FAILURE; }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (log_message("parent", "child=%ld exited status=%d; reaped", (long)pid, code) == -1)
            return EXIT_FAILURE;
        return code;
    }
    if (log_message("parent", "child=%ld terminated by signal=%d; reaped", (long)pid,
                    WIFSIGNALED(status) ? WTERMSIG(status) : 0) == -1) perror("log");
    return EXIT_FAILURE;
}
