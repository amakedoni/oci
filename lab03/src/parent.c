
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void kmeans(double points[][2], int n, int k, int t);

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-t") != 0) {
        printf("Использование: %s -t число_потоков\n", argv[0]);
        return 1;
    }

    int t = atoi(argv[2]);
    int n, k;

    if (t <= 0) {
        printf("Некорректное количество потоков\n");
        return 1;
    }

    printf("Введите количество точек: ");
    if (scanf("%d", &n) != 1 || n <= 0)
        return 1;

    double (*points)[2] = malloc(n * sizeof(*points));

    if (points == NULL) {
        perror("Ошибка выделения памяти");
        return 1;
    }

    printf("Введите координаты точек x y:\n");

    for (int i = 0; i < n; i++) {
        if (scanf("%lf %lf", &points[i][0],
                  &points[i][1]) != 2) {
            free(points);
            return 1;
        }
    }

    printf("Введите количество кластеров: ");

    if (scanf("%d", &k) != 1 || k <= 0 || k > n) {
        free(points);
        return 1;
    }

    if (t > n)
        t = n;

    printf("PID процесса: %d\n", getpid());
    printf("Количество потоков: %d\n", t);

    kmeans(points, n, k, t);

    free(points);

    return 0;
}
