#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *child_path(const char *parent)
{
    const char *slash = strrchr(parent, '/');
    size_t prefix = slash == NULL ? 0 : (size_t)(slash - parent) + 1;
    char *path = malloc(prefix + sizeof "child");
    if (path != NULL) {
        memcpy(path, parent, prefix);
        memcpy(path + prefix, "child", sizeof "child");
    }
    return path;
}

static int close_pipes(int commands[2], int replies[2])
{
    int failed = 0;
    if (close_fd(&commands[0]) == -1) failed = 1;
    if (close_fd(&commands[1]) == -1) failed = 1;
    if (close_fd(&replies[0]) == -1) failed = 1;
    if (close_fd(&replies[1]) == -1) failed = 1;
    return failed ? -1 : 0;
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fputs("Usage: parent [child_executable]\n", stderr);
        return 1;
    }
    if (setup_signals() == -1) {
        perror("parent: sigaction");
        return 1;
    }
    int result = EXIT_FAILURE;
    int commands[2] = {-1, -1}, replies[2] = {-1, -1};
    pid_t child = -1;
    char *filename = NULL, *program = NULL;
    size_t length;
    if (isatty(STDIN_FILENO) && dprintf(STDERR_FILENO, "[parent pid=%ld] Enter output filename, then float commands; Ctrl-D to finish\n", (long)getpid()) < 0)
        goto cleanup;
    int status = read_line(STDIN_FILENO, -1, &filename, &length);
    if (status == -1) {
        perror("parent: read filename");
        goto cleanup;
    }
    if (length != 0 && filename[length - 1] == '\n') filename[--length] = '\0';
    if (length != 0 && filename[length - 1] == '\r') filename[--length] = '\0';
    if (status == 0 || length == 0 || memchr(filename, '\0', length) != NULL) {
        if (dprintf(STDERR_FILENO, "[parent pid=%ld] output filename is empty or contains NUL\n", (long)getpid()) < 0) perror("log");
        goto cleanup;
    }
    program = argc == 2 ? strdup(argv[1]) : child_path(argv[0]);
    if (program == NULL) {
        perror("parent: malloc path");
        goto cleanup;
    }
    if (pipe(commands) == -1) {
        perror("parent: pipe1");
        goto cleanup;
    }
    if (pipe(replies) == -1) {
        perror("parent: pipe2");
        goto cleanup;
    }
    child = fork();
    if (child == -1) {
        perror("parent: fork");
        goto cleanup;
    }
    if (child == 0) {
        if (dup2(commands[0], STDIN_FILENO) == -1 ||
            dup2(replies[1], STDOUT_FILENO) == -1) { perror("child: dup2"); _exit(1); }
        if (close_pipes(commands, replies) == -1) _exit(1);
        char *const arguments[] = {program, filename, NULL};
        execvp(program, arguments);
        perror("child: execvp");
        _exit(127);
    }
    if (close_fd(&commands[0]) == -1 || close_fd(&replies[1]) == -1) goto cleanup;
    if (dprintf(STDERR_FILENO, "[parent pid=%ld] child=%ld; pipe1 sends commands, pipe2 receives status\n", (long)getpid(), (long)child) < 0)
        goto cleanup;
    unsigned char reply;
    if (receive_reply(replies[0], &reply) == -1) {
        perror("parent: startup reply");
        goto cleanup;
    }
    if (reply != READY) goto cleanup;
    result = EXIT_SUCCESS;
    for (;;) {
        char *line = NULL;
        status = read_line(STDIN_FILENO, replies[0], &line, &length);
        if (status == 0) {
            free(line);
            break;
        }
        if (status < 0) {
            if (status == -1) perror("parent: read command");
            else if (dprintf(STDERR_FILENO, "[parent pid=%ld] child channel closed while waiting for input\n", (long)getpid()) < 0) perror("log");
            result = EXIT_FAILURE;
            break;
        }
        int failed = write_all(commands[1], line, length);
        if (failed == 0 && line[length - 1] != '\n') failed = write_all(commands[1], "\n", 1);
        free(line);
        if (failed == -1) {
            perror("parent: pipe1 write");
            result = EXIT_FAILURE;
            break;
        }
        if (receive_reply(replies[0], &reply) == -1) {
            perror("parent: reply read");
            result = EXIT_FAILURE;
            break;
        }
        if (reply == ZERO) {
            if (dprintf(STDERR_FILENO, "[parent pid=%ld] division by zero reported by child; stopping\n", (long)getpid()) < 0) perror("log");
            result = EXIT_ZERO_DIVISION;
            break;
        }
        if (reply == FAILED || (reply != ACCEPTED && reply != INVALID)) {
            result = EXIT_FAILURE;
            break;
        }
    }
cleanup:
    if (close_pipes(commands, replies) == -1) result = EXIT_FAILURE;
    if (child > 0) {
        if (interrupted && kill(child, SIGTERM) == -1 && errno != ESRCH) perror("kill child");
        int child_result = reap_child(child);
        if (child_result != EXIT_SUCCESS) result = child_result;
    }
    free(program);
    free(filename);
    if (interrupted) result = 128 + interrupted;
    return result;
}
