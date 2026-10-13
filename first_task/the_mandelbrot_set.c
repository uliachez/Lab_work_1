
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    double a;
    double b;
} Point;

typedef struct {
    int id;
    size_t start;
    size_t count;
    Point* points;
} ThreadArgs;

static int is_mandelbrot(double a, double b) {
    double x = 0.0;
    double y = 0.0;

    const int max_iter = 1000;

    for (int i = 0; i < max_iter; i++) {
        double new_x = x * x - y * y + a;
        double new_y = 2.0 * x * y + b;

        x = new_x;
        y = new_y;

        if (x * x + y * y > 4.0) {
            return 0;
        }
    }

    return 1;
}

static void* thread_work(void* arg) {
    ThreadArgs* data = (ThreadArgs*)arg;

    unsigned seed = (unsigned)(data->id + 1);
    size_t found = 0;

    while (found < data->count) {
        double a = -2.0 + 3.0 * (double)rand_r(&seed) / RAND_MAX;
        double b = -1.5 + 3.0 * (double)rand_r(&seed) / RAND_MAX;

        if (is_mandelbrot(a, b)) {
            size_t index = data->start + found;

            data->points[index].a = a;
            data->points[index].b = b;

            found++;
        }
    }

    return NULL;
}

static int parse_positive(const char* text, long* result) {
    char* end;

    errno = 0;
    long number = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || number <= 0) {
        return 0;
    }

    *result = number;
    return 1;
}

int main(int argc, char* argv[]) {
    long thread_input;
    long point_input;

    if (argc != 3 ||
        !parse_positive(argv[1], &thread_input) ||
        !parse_positive(argv[2], &point_input) ||
        thread_input > INT_MAX) {

        fprintf(stderr,
                "Usage: %s nthreads npoints (positive integers)\n",
                argv[0]);
        return 1;
    }

    size_t nthreads = (size_t)thread_input;
    size_t npoints = (size_t)point_input;

    if (nthreads > npoints) {
        nthreads = npoints;
    }

    if (npoints > SIZE_MAX / sizeof(Point) ||
        nthreads > SIZE_MAX / sizeof(pthread_t) ||
        nthreads > SIZE_MAX / sizeof(ThreadArgs)) {

        fprintf(stderr, "Requested size is too large\n");
        return 1;
    }

    Point* points = malloc(npoints * sizeof(Point));
    pthread_t* threads = malloc(nthreads * sizeof(pthread_t));
    ThreadArgs* args = malloc(nthreads * sizeof(ThreadArgs));

    if (points == NULL || threads == NULL || args == NULL) {
        fprintf(stderr, "Out of memory\n");

        free(points);
        free(threads);
        free(args);

        return 1;
    }

    size_t base = npoints / nthreads;
    size_t remainder = npoints % nthreads;

    size_t start = 0;
    size_t created = 0;
    int failed = 0;

    struct timespec start_time;
    struct timespec end_time;

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        perror("clock_gettime");

        free(points);
        free(threads);
        free(args);

        return 1;
    }

    for (size_t i = 0; i < nthreads; i++) {
        size_t count = base;

        if (i < remainder) {
            count++;
        }

        args[i].id = (int)i;
        args[i].start = start;
        args[i].count = count;
        args[i].points = points;

        int result = pthread_create(
            &threads[i],
            NULL,
            thread_work,
            &args[i]
        );

        if (result != 0) {
            fprintf(stderr,
                    "pthread_create failed (code %d)\n",
                    result);
            failed = 1;
            break;
        }

        created++;
        start += count;
    }

    for (size_t i = 0; i < created; i++) {
        int result = pthread_join(threads[i], NULL);

        if (result != 0) {
            fprintf(stderr,
                    "pthread_join failed (code %d)\n",
                    result);
            failed = 1;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        perror("clock_gettime");
        failed = 1;
    } else if (!failed) {
        double elapsed =
            (double)(end_time.tv_sec - start_time.tv_sec) +
            (double)(end_time.tv_nsec - start_time.tv_nsec) / 1e9;

        printf("Compute time: %.6f seconds\n", elapsed);
    }

    if (!failed) {
        FILE* file = fopen("points.csv", "w");

        if (file == NULL) {
            perror("points.csv");
            failed = 1;
        } else {
            if (fprintf(file, "x,y\n") < 0) {
                failed = 1;
            }

            for (size_t i = 0; i < npoints && !failed; i++) {
                if (fprintf(file, "%.10f,%.10f\n",
                            points[i].a, points[i].b) < 0) {
                    failed = 1;
                }
            }

            if (fclose(file) != 0) {
                failed = 1;
            }

            if (failed) {
                fprintf(stderr, "Error writing CSV\n");
            } else {
                printf("Saved %zu points using %zu threads to points.csv\n",
                       npoints, nthreads);
            }
        }
    }

    free(points);
    free(threads);
    free(args);

    return failed ? 1 : 0;
}
