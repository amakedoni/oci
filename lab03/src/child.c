
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

typedef struct {
    double (*points)[2];
    double (*centers)[2];
    int *labels;
    int start;
    int end;
    int k;
} Data;

void *worker(void *arg) {
    Data *d = arg;

    for (int i = d->start; i < d->end; i++) {
        double min = 0;
        int best = 0;

        for (int j = 0; j < d->k; j++) {
            double dx = d->points[i][0] - d->centers[j][0];
            double dy = d->points[i][1] - d->centers[j][1];
            double dist = dx * dx + dy * dy;

            if (j == 0 || dist < min) {
                min = dist;
                best = j;
            }
        }

        d->labels[i] = best;
    }

    return NULL;
}

int kmeans(double points[][2], int n, int k, int t) {
    int status = EXIT_FAILURE;
    double (*centers)[2] = malloc(k * sizeof(*centers));
    double (*sum)[2] = malloc(k * sizeof(*sum));
    int *labels = malloc(n * sizeof(int));
    int *count = malloc(k * sizeof(int));
    pthread_t *threads = malloc(t * sizeof(pthread_t));
    Data *data = malloc(t * sizeof(Data));

    if (!centers || !sum || !labels ||
        !count || !threads || !data) {
        perror("Ошибка выделения памяти");
        goto cleanup;
    }

    for (int j = 0; j < k; j++) {
        centers[j][0] = points[j][0];
        centers[j][1] = points[j][1];
    }

    struct timespec start, end;

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("Ошибка получения времени");
        goto cleanup;
    }

    for (int iter = 0; iter < 100; iter++) {
        int created = 0;

        for (int i = 0; i < t; i++) {
            data[i] = (Data){
                points, centers, labels,
                (int)((long long)n * i / t),
                (int)((long long)n * (i + 1) / t), k
            };
        }

        for (int i = 0; i < t - 1; i++) {
            int err = pthread_create(
                &threads[i], NULL, worker, &data[i]
            );

            if (err != 0) {
                fprintf(stderr,
                        "Ошибка создания потока: %s\n",
                        strerror(err));
                break;
            }

            created++;
        }

        for (int i = created; i < t; i++)
            worker(&data[i]);

        for (int i = 0; i < created; i++) {
            int err = pthread_join(threads[i], NULL);

            if (err != 0) {
                fprintf(stderr,
                        "Ошибка ожидания потока: %s\n",
                        strerror(err));
                exit(1);
            }
        }

        for (int j = 0; j < k; j++) {
            sum[j][0] = 0;
            sum[j][1] = 0;
            count[j] = 0;
        }

        for (int i = 0; i < n; i++) {
            int j = labels[i];

            sum[j][0] += points[i][0];
            sum[j][1] += points[i][1];
            count[j]++;
        }

        int changed = 0;

        for (int j = 0; j < k; j++) {
            if (count[j] == 0)
                continue;

            double x = sum[j][0] / count[j];
            double y = sum[j][1] / count[j];

            if (x != centers[j][0] || y != centers[j][1])
                changed = 1;

            centers[j][0] = x;
            centers[j][1] = y;
        }

        if (!changed)
            break;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("Ошибка получения времени");
        goto cleanup;
    }

    double time = end.tv_sec - start.tv_sec +
        (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\nРезультаты кластеризации:\n");

    for (int j = 0; j < k; j++) {
        printf("Кластер %d: центр (%.2f, %.2f)\n", j + 1, centers[j][0], centers[j][1]);
    }

    printf("\nВремя выполнения: %.9f секунд\n", time);
    status = EXIT_SUCCESS;

cleanup:
    free(centers);
    free(sum);
    free(labels);
    free(count);
    free(threads);
    free(data);
    return status;
}
