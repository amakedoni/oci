#include "common.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    if (setup_signals() == -1) { perror("sigaction"); return 1; }
    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fputs("parent: cannot disable input buffering\n", stderr);
        return 1;
    }

    int result = EXIT_FAILURE;
    int commands[2] = {-1, -1}, replies[2] = {-1, -1};
    pid_t child = -1;
    char *filename = NULL;
    size_t capacity = 0;
    ssize_t length = getline(&filename, &capacity, stdin);
    if (length == -1) {
        if (feof(stdin)) fputs("parent: missing output filename\n", stderr);
        else perror("parent: getline");
        goto cleanup;
    }
    if (length > 0 && filename[length - 1] == '\n') filename[--length] = '\0';
    if (length > 0 && filename[length - 1] == '\r') filename[--length] = '\0';
    if (length == 0 || memchr(filename, '\0', (size_t)length) != NULL) {
        fputs("parent: invalid output filename\n", stderr);
        goto cleanup;
    }

    if (pipe(commands) == -1) { perror("pipe1"); goto cleanup; }
    if (pipe(replies) == -1) { perror("pipe2"); goto cleanup; }
    child = fork();
    if (child == -1) { perror("fork"); goto cleanup; }
    if (child == 0) {
        if (dup2(commands[0], STDIN_FILENO) == -1 ||
            dup2(replies[1], STDOUT_FILENO) == -1) { perror("dup2"); _exit(1); }
        if (close_pipes(commands, replies) == -1) _exit(1);
        char *program = argc == 2 ? argv[1] : "./src/build/child";
        char *arguments[] = {program, filename, NULL};
        execvp(program, arguments);
        perror("execvp");
        _exit(127);
    }
    if (close_fd(&commands[0]) == -1 || close_fd(&replies[1]) == -1) goto cleanup;
    if (dprintf(STDERR_FILENO, "[parent pid=%ld] child=%ld\n",
                (long)getpid(), (long)child) < 0) { perror("parent: log"); goto cleanup; }
    int reply = receive_reply(replies[0]);
    if (reply != 'A') {
        if (reply >= 0) errno = EPROTO;
        perror("parent: child startup");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
    int pending = 0;
    while (!interrupted) {
        struct pollfd fds[2] = {{STDIN_FILENO, POLLIN, 0}, {replies[0], POLLIN, 0}};
        int ready;
        do { ready = poll(fds, 2, 100); }
        while (ready == -1 && errno == EINTR && !interrupted);
        if (ready == 0) continue;
        if (ready == -1 || fds[1].revents != 0) {
            fputs("parent: input interrupted or child channel closed\n", stderr);
            result = EXIT_FAILURE;
            break;
        }
        char byte;
        int count = read_byte(STDIN_FILENO, &byte);
        if (count == -1) { perror("parent: read"); result = EXIT_FAILURE; break; }
        if (count == 0) {
            if (!pending) break;
            byte = '\n';
        }
        if (write_byte(commands[1], byte) == -1) {
            perror("parent: write"); result = EXIT_FAILURE; break;
        }
        pending = byte != '\n';
        if (pending) continue;
        reply = receive_reply(replies[0]);
        if (reply == 'Z') { result = EXIT_ZERO_DIVISION; break; }
        if (reply != 'A') {
            if (reply >= 0) errno = EPROTO;
            perror("parent: reply"); result = EXIT_FAILURE; break;
        }
        if (count == 0) break;
    }
cleanup:
    if (close_pipes(commands, replies) == -1) result = EXIT_FAILURE;
    if (child > 0) {
        int code = reap_child(child);
        if (code != EXIT_SUCCESS) result = code;
    }
    free(filename);
    if (interrupted) result = 128 + interrupted;
    return result;
}
