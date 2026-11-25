
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

enum { DIVISION_BY_ZERO = 2 };

static int divide_line(char *line, int output) {
    char *number = strtok(line, " \t\n\r");
    if (number == NULL)
        return 0;

    float result = strtof(number, NULL);
    while ((number = strtok(NULL, " \t\n\r")) != NULL) {
        float divisor = strtof(number, NULL);
        if (divisor == 0)
            return DIVISION_BY_ZERO;

        result /= divisor;
    }

    if (dprintf(output, "%g\n", result) < 0) {
        perror("Ошибка записи в файл");
        return 1;
    }
    return 0;
}

static int process_line(int fd, const char *input, size_t length, int output) {
    off_t size = (off_t)length;
    if (size < 0 || (size_t)size != length) {
        fprintf(stderr, "Слишком длинная строка\n");
        return 1;
    }
    if (ftruncate(fd, size) == -1) {
        perror("Ошибка ftruncate");
        return 1;
    }

    char *line = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (line == MAP_FAILED) {
        perror("Ошибка mmap");
        return 1;
    }
    memcpy(line, input, length);

    pid_t pid = fork();
    if (pid == 0) {
        printf("PID дочернего процесса: %d\n", (int)getpid());
        if (fflush(stdout) == EOF) {
            perror("Ошибка fflush");
            _exit(1);
        }
        _exit(divide_line(line, output));
    }

    int result = 1;
    if (pid == -1) {
        perror("Ошибка fork");
    } else {
        int status;
        pid_t waited;
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1)
            perror("Ошибка waitpid");
        else if (WIFEXITED(status))
            result = WEXITSTATUS(status);
        else
            fprintf(stderr, "Ошибка дочернего процесса\n");
    }

    if (munmap(line, length) == -1) {
        perror("Ошибка munmap");
        return 1;
    }
    return result;
}

int main(void) {
    char *input = NULL;
    size_t capacity = 0;
    int output = -1;
    FILE *temp = NULL;
    int error = 1;

    printf("PID родительского процесса: %d\n", (int)getpid());
    puts("Введите имя файла:");
    if (getline(&input, &capacity, stdin) == -1) {
        fprintf(stderr, "Ошибка ввода имени файла\n");
        goto cleanup;
    }
    input[strcspn(input, "\n")] = '\0';
    output = open(input, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output == -1) {
        perror("Ошибка открытия файла");
        goto cleanup;
    }

    temp = tmpfile();
    if (temp == NULL) {
        perror("Ошибка tmpfile");
        goto cleanup;
    }
    int fd = fileno(temp);
    if (fd == -1) {
        perror("Ошибка fileno");
        goto cleanup;
    }

    puts("Введите числа:");
    if (fflush(stdout) == EOF) {
        perror("Ошибка fflush");
        goto cleanup;
    }

    error = 0;
    ssize_t length;
    while ((length = getline(&input, &capacity, stdin)) != -1) {
        int status = process_line(fd, input, (size_t)length + 1, output);
        if (status == DIVISION_BY_ZERO) {
            puts("Ошибка: деление на ноль");
            break;
        }
        if (status != 0) {
            error = 1;
            break;
        }
    }
    if (length == -1 && !feof(stdin)) {
        perror("Ошибка getline");
        error = 1;
    }
    puts("Родительский процесс завершён");

cleanup:
    free(input);
    if (temp != NULL && fclose(temp) == EOF) {
        perror("Ошибка закрытия временного файла");
        error = 1;
    }
    if (output != -1 && close(output) == -1) {
        perror("Ошибка закрытия выходного файла");
        error = 1;
    }
    return error;
}
