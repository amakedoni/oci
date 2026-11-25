

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Не указано имя файла\n");
        return 1;
    }

    fprintf(stderr, "PID дочернего процесса: %d\n", (int)getpid());

    FILE *file = fopen(argv[1], "w");

    if (file == NULL) {
        perror("Ошибка открытия файла");
        return 1;
    }

    char *line = NULL;
    size_t size = 0;
    int error = 0;

    while (getline(&line, &size, stdin) != -1) {
        char *token = strtok(line, " \t\n\r");
        char answer = '0';

        if (token != NULL) {
            float result = strtof(token, NULL);

            token = strtok(NULL, " \t\n\r");

            while (token != NULL) {
                float number = strtof(token, NULL);

                if (number == 0) {
                    answer = '1';
                    break;
                }

                result /= number;

                token = strtok(NULL, " \t\n\r");
            }

            if (answer == '0') {
                if (fprintf(file, "%g\n", result) < 0 ||
                    fflush(file) == EOF) {
                    perror("Ошибка записи в файл");
                    error = 1;
                    break;
                }
            }
        }

        ssize_t n;

        do {
            n = write(STDOUT_FILENO, &answer, 1);
        } while (n == -1 && errno == EINTR);

        if (n != 1) {
            perror("Ошибка write");
            error = 1;
            break;
        }

        if (answer == '1')
            break;
    }

    if (ferror(stdin)) {
        perror("Ошибка getline");
        error = 1;
    }

    free(line);

    if (fclose(file) == EOF) {
        perror("Ошибка закрытия файла");
        error = 1;
    }

    fprintf(stderr, "Дочерний процесс завершён\n");

    return error;
}
