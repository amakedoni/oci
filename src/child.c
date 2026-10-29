#include "common.h"
#include "division.h"

#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int send_reply(enum reply reply)
{
    unsigned char byte = (unsigned char)reply;
    if (write_all(STDOUT_FILENO, &byte, 1) == -1) {
        perror("child: pipe2 write");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fputs("Usage: child output_file (stdin=pipe1, stdout=pipe2)\n", stderr);
        return 1;
    }
    if (setup_signals() == -1) {
        perror("child: sigaction");
        return 1;
    }
    int result = EXIT_FAILURE;
    int output = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output == -1) {
        perror("child: open output");
        if (send_reply(FAILED) == -1) perror("notify");
        return 1;
    }
    if (dprintf(STDERR_FILENO, "[child pid=%ld] ppid=%ld; output=%s; ready\n", (long)getpid(), (long)getppid(), argv[1]) < 0) goto cleanup;
    if (send_reply(READY) == -1) goto cleanup;
    size_t number = 0;
    for (;;) {
        char *line = NULL;
        size_t size;
        int status = read_line(STDIN_FILENO, -1, &line, &size);
        if (status == -1) {
            perror("child: read line");
            if (send_reply(FAILED) == -1) perror("notify");
            goto cleanup;
        }
        if (status == 0) {
            free(line);
            result = EXIT_SUCCESS;
            break;
        }
        ++number;
        float value = 0.0f;
        enum calculation calculation = memchr(line, '\0', size) != NULL
            ? CALC_INVALID : divide_line(line, &value);
        free(line);
        if (calculation == CALC_ZERO) {
            if (dprintf(STDERR_FILENO, "[child pid=%ld] line=%zu: division by zero; stopping\n", (long)getpid(), number) < 0) goto cleanup;
            if (send_reply(ZERO) == -1) goto cleanup;
            result = EXIT_ZERO_DIVISION;
            break;
        }
        if (calculation == CALC_INVALID || calculation == CALC_RANGE) {
            if (dprintf(STDERR_FILENO, "[child pid=%ld] line=%zu: %s; skipped\n", (long)getpid(), number,
                            calculation == CALC_INVALID ? "invalid float token" : "float out of range") < 0)
                goto cleanup;
            if (send_reply(INVALID) == -1) goto cleanup;
            continue;
        }
        if (calculation == CALC_OK) {
            if (dprintf(output, "%.*g\n", FLT_DECIMAL_DIG, (double)value) < 0) {
                perror("child: write output");
                if (send_reply(FAILED) == -1) perror("notify");
                goto cleanup;
            }
            if (dprintf(STDERR_FILENO, "[child pid=%ld] line=%zu: result=%.*g\n", (long)getpid(), number, FLT_DECIMAL_DIG, (double)value) < 0)
                goto cleanup;
        }
        if (send_reply(ACCEPTED) == -1) goto cleanup;
    }
cleanup:
    if (close_fd(&output) == -1) result = EXIT_FAILURE;
    int in = STDIN_FILENO, out = STDOUT_FILENO;
    if (close_fd(&in) == -1) result = EXIT_FAILURE;
    if (close_fd(&out) == -1) result = EXIT_FAILURE;
    if (interrupted) result = 128 + interrupted;
    return result;
}
