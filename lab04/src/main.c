
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

enum { DIVISION_BY_ZERO = 2 };

static int fail(const char *message) {
    perror(message);
    return 1;
}

static int divide_line(char *line, int output) {
    char *number = strtok(line, " \t\n\r");

    if (number == NULL) {
        return 0;
    }

    float result = strtof(number, NULL);

    while ((number = strtok(NULL, " \t\n\r")) != NULL) {
        float divisor = strtof(number, NULL);

        if (divisor == 0) {
            return DIVISION_BY_ZERO;
        }

        result /= divisor;
    }

    if (dprintf(output, "%g\n", result) < 0) {
        return fail("Ошибка записи");
    }

    return 0;
}

static int process_line(int fd, char *input, size_t length, int output) {
    if (length > INT64_MAX) {
        fprintf(stderr, "Слишком длинная строка\n");
        return 1;
    }

    if (ftruncate(fd, length) == -1) {
        return fail("Ошибка ftruncate");
    }

    char *line = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (line == MAP_FAILED) {
        return fail("Ошибка mmap");
    }

    memcpy(line, input, length);

    pid_t pid = fork();
    int result = 1;

    if (pid == 0) {
        printf("PID дочернего процесса: %d\n", (int)getpid());

        if (fflush(stdout) == EOF) {
            result = fail("Ошибка fflush");
        } else {
            result = divide_line(line, output);
        }
    } else if (pid == -1) {
        perror("Ошибка fork");
    } else {
        int status;
        pid_t waited;

        do {
            waited = waitpid(pid, &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1) {
            perror("Ошибка waitpid");
        } else if (WIFEXITED(status) != 0) {
            result = WEXITSTATUS(status);
        } else {
            fprintf(stderr, "Ошибка дочернего процесса\n");
        }
    }

    if (munmap(line, length) == -1) {
        result = fail("Ошибка munmap");
    }

    if (pid == 0) {
        _exit(result);
    }

    return result;
}

int main(void) {
    char *input = NULL;
    size_t capacity = 0;
    int output = -1;
    int error = 1;
    FILE *temp = NULL;

    printf("PID родительского процесса: %d\n", (int)getpid());
    printf("Введите имя файла:\n");

    if (getline(&input, &capacity, stdin) == -1) {
        fprintf(stderr, "Ошибка ввода имени файла\n");
        goto cleanup;
    }

    input[strcspn(input, "\n")] = '\0';

    output = open(input, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (output == -1) {
        goto failed;
    }

    temp = tmpfile();

    if (temp == NULL) {
        goto failed;
    }

    int fd = fileno(temp);

    if (fd == -1) {
        goto failed;
    }

    printf("Введите числа:\n");

    if (fflush(stdout) == EOF) {
        goto failed;
    }

    error = 0;
    ssize_t length;

    while ((length = getline(&input, &capacity, stdin)) != -1) {
        int status = process_line(fd, input, (size_t)length + 1, output);

        if (status == DIVISION_BY_ZERO) {
            printf("Ошибка: деление на ноль\n");
            break;
        }

        if (status != 0) {
            error = 1;
            break;
        }
    }

    if (length == -1 && feof(stdin) == 0) {
        goto failed;
    }

    printf("Родительский процесс завершён\n");
    goto cleanup;

failed:
    error = fail("Системная ошибка");

cleanup:
    free(input);

    if (temp != NULL && fclose(temp) == EOF) {
        error = fail("Ошибка fclose");
    }

    if (output != -1 && close(output) == -1) {
        error = fail("Ошибка close");
    }

    return error;
}
