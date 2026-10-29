#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum calculation { CALC_OK, CALC_EMPTY, CALC_INVALID, CALC_ZERO };

static enum calculation divide_line(const char *line, float *result)
{
    int first = 1;
    while (*line != '\0') {
        while (isspace((unsigned char)*line)) ++line;
        if (*line == '\0') break;
        char *end;
        errno = 0;
        float number = strtof(line, &end);
        if (end == line || (*end != '\0' && !isspace((unsigned char)*end)))
            return CALC_INVALID;
        if (errno == ERANGE || !isfinite(number)) return CALC_INVALID;
        if (first) { *result = number; first = 0; }
        else {
            if (number == 0.0f) return CALC_ZERO;
            *result /= number;
            if (!isfinite(*result)) return CALC_INVALID;
        }
        line = end;
    }
    return first ? CALC_EMPTY : CALC_OK;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fputs("Usage: child output_file\n", stderr); return 1; }
    if (setup_signals() == -1) { perror("sigaction"); return 1; }
    int result = EXIT_FAILURE;
    int output = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char *line = NULL;
    size_t capacity = 0;
    if (output == -1) { perror("child: open output"); goto cleanup; }
    if (dprintf(STDERR_FILENO, "[child pid=%ld] ppid=%ld; ready\n",
                (long)getpid(), (long)getppid()) < 0) { perror("child: log"); goto cleanup; }
    if (write_byte(STDOUT_FILENO, 'A') == -1) { perror("child: reply"); goto cleanup; }

    while (!interrupted) {
        ssize_t length = getline(&line, &capacity, stdin);
        if (length == -1) {
            if (feof(stdin)) result = EXIT_SUCCESS;
            else perror("child: getline");
            break;
        }
        float value = 0.0f;
        enum calculation status = memchr(line, '\0', (size_t)length) != NULL
            ? CALC_INVALID : divide_line(line, &value);
        if (status == CALC_ZERO) {
            if (dprintf(STDERR_FILENO, "[child pid=%ld] division by zero\n",
                        (long)getpid()) < 0) { perror("child: log"); goto cleanup; }
            if (write_byte(STDOUT_FILENO, 'Z') == -1) { perror("child: reply"); goto cleanup; }
            result = EXIT_ZERO_DIVISION;
            break;
        }
        if (status == CALC_INVALID) {
            if (dprintf(STDERR_FILENO, "[child pid=%ld] invalid input; skipped\n",
                        (long)getpid()) < 0) { perror("child: log"); goto cleanup; }
        }
        if (status == CALC_OK && dprintf(output, "%.*g\n", FLT_DECIMAL_DIG, (double)value) < 0) {
            perror("child: write output");
            goto cleanup;
        }
        if (write_byte(STDOUT_FILENO, 'A') == -1) { perror("child: reply"); goto cleanup; }
    }
cleanup:
    free(line);
    if (close_fd(&output) == -1) result = EXIT_FAILURE;
    int in = STDIN_FILENO, out = STDOUT_FILENO;
    if (close_fd(&in) == -1) result = EXIT_FAILURE;
    if (close_fd(&out) == -1) result = EXIT_FAILURE;
    if (interrupted) result = 128 + interrupted;
    return result;
}
