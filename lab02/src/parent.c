
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

int close_fd(int fd) {
    if (close(fd) == -1) {
        perror("Ошибка close");
        return 1;
    }
    return 0;
}

int send_line(int fd, char *line, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, line, len);

        if (n == -1 && errno == EINTR)
            continue;

        if (n <= 0) {
            perror("Ошибка write");
            return 1;
        }

        line += n;
        len -= n;
    }
    return 0;
}

int main(void) {
    int p1[2], p2[2], status;
    int error = 0;
    char filename[256];
    char *line = NULL;
    size_t size = 0;

    printf("PID родительского процесса: %d\n", (int)getpid());
    printf("Введите имя файла:\n");

    if (!fgets(filename, sizeof(filename), stdin)) {
        fprintf(stderr, "Ошибка ввода имени файла\n");
        return 1;
    }

    if (!strchr(filename, '\n')) {
        fprintf(stderr, "Слишком длинное или неполное имя файла\n");
        return 1;
    }

    filename[strcspn(filename, "\n")] = '\0';

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("Ошибка signal");
        return 1;
    }

    if (pipe(p1) == -1) {
        perror("Ошибка pipe");
        return 1;
    }

    if (pipe(p2) == -1) {
        perror("Ошибка pipe");
        close_fd(p1[0]);
        close_fd(p1[1]);
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("Ошибка fork");
        close_fd(p1[0]);
        close_fd(p1[1]);
        close_fd(p2[0]);
        close_fd(p2[1]);
        return 1;
    }

    if (pid == 0) {
        if (close_fd(p1[1]) | close_fd(p2[0]))
            _exit(1);

        if (dup2(p1[0], STDIN_FILENO) == -1 ||
            dup2(p2[1], STDOUT_FILENO) == -1) {
            perror("Ошибка dup2");
            _exit(1);
        }

        if (close_fd(p1[0]) | close_fd(p2[1]))
            _exit(1);

        execl("./child", "child", filename, NULL);
        perror("Ошибка execl");
        _exit(1);
    }

    error |= close_fd(p1[0]);
    error |= close_fd(p2[1]);

    printf("Введите числа:\n");

    ssize_t len;

    while (!error && (len = getline(&line, &size, stdin)) != -1) {
        if (send_line(p1[1], line, len) ||
            (line[len - 1] != '\n' &&
             send_line(p1[1], "\n", 1))) {
            error = 1;
            break;
        }

        char answer;
        ssize_t n;

        do {
            n = read(p2[0], &answer, 1);
        } while (n == -1 && errno == EINTR);

        if (n != 1) {
            if (n == -1)
                perror("Ошибка read");
            else
                fprintf(stderr, "Дочерний процесс завершился\n");

            error = 1;
            break;
        }

        if (answer == '1') {
            printf("Ошибка: деление на ноль\n");
            break;
        }
    }

    if (!error && !feof(stdin)) {
        perror("Ошибка getline");
        error = 1;
    }

    free(line);
    error |= close_fd(p1[1]);
    error |= close_fd(p2[0]);

    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("Ошибка waitpid");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Ошибка дочернего процесса\n");
        return 1;
    }

    printf("Родительский процесс завершён\n");

    return error;
}
