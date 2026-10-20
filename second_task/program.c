#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define G 6.67430e-11 // гравитационная постоянная
#define DT 0.01 // шаг времени
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// данные тела
typedef struct {
    double m; // масса
    double r[3]; // координаты 
    double v[3]; // скорость 
} Body;

// данные для потока
typedef struct {
    int id; 
    int start; 
    int end; 
    int n; // количество тел
    int step; 
    Body *bodies; // указатель на общий массив тел
    double *forces; // указатель на массив сил (для этого потока)
} ThreadData;

Body *bodies; // общий массив тел
double *forces; // итоговые силы тел
double **local_forces; // массив указателей на локальные силы для каждого потока
pthread_t *threads; // массив потоков
ThreadData *data; // массив данных для потоков
int n; // количество тел
int num_threads; // количество потоков

void *calculate_forces(void *arg)
{
    ThreadData *d = (ThreadData *)arg;
    double *f = d->forces;
    long long pair_count = 0;

    /*pthread_mutex_lock(&print_mutex);
    printf("[START] Поток %d: тела i = %d..%d, шаг %d\n", d->id, d->start, d->end - 1, d->step);
    pthread_mutex_unlock(&print_mutex);*/

    for (int i = d->start; i < d->end; i++) {
        for (int j = i + 1; j < d->n; j++) {
            double dx = d->bodies[j].r[0] - d->bodies[i].r[0];
            double dy = d->bodies[j].r[1] - d->bodies[i].r[1];
            double dz = d->bodies[j].r[2] - d->bodies[i].r[2];

            double r2 = dx * dx + dy * dy + dz * dz; // квадрат расстояния между телами

            if (r2 < 1e-20)
                continue;

            double factor = G * d->bodies[i].m * d->bodies[j].m / (r2 * sqrt(r2)); // закон всемирного тяготения (коэффициент силы)

            //компоненты вектора силы
            double fx = factor * dx;
            double fy = factor * dy;
            double fz = factor * dz;

            f[3 * i] += fx;
            f[3 * i + 1] += fy;
            f[3 * i + 2] += fz;

            f[3 * j] -= fx; 
            f[3 * j + 1] -= fy;
            f[3 * j + 2] -= fz;
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) 
// argv[0] — имя программы 
// argv[1] — время окончания моделирования
// argv[2] — имя входного файла
// argv[3] — колво потоков
{   
    if (argc != 4) {
        fprintf(stderr, "There should be four arguments (program name, tend, input_file, num_threads)\n");
        return 1;
    }

    char *endptr;
    double tend = strtod(argv[1], &endptr);

    if (*endptr != '\0' || !isfinite(tend) || tend < 0) {
        fprintf(stderr, "Invalid tend\n");
        return 1;
    }

    num_threads = (int)strtod(argv[3], &endptr);

    //num_threads = (int)sysconf(_SC_NPROCESSORS_ONLN); // количество доступных процессов

    if (*endptr != '\0' || num_threads <= 0) {
        fprintf(stderr, "Invalid number of threads\n");
        return 1;
    }

    FILE *in = fopen(argv[2], "r");
    if (in == NULL) {
        perror("Cannot open input file");
        return 1;
    }

    if (fscanf(in, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid number of bodies\n");
        fclose(in);
        return 1;
    }

    if (num_threads < 1)
        num_threads = 1;
    if (num_threads > n)
        num_threads = n;

    bodies = calloc((size_t)n, sizeof(Body));
    forces = calloc((size_t)n * 3, sizeof(double));

    if (bodies == NULL || forces == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(in);
        free(bodies);
        free(forces);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        if (fscanf(in, "%lf %lf %lf %lf %lf %lf %lf",
                   &bodies[i].m,
                   &bodies[i].r[0],
                   &bodies[i].r[1],
                   &bodies[i].r[2],
                   &bodies[i].v[0],
                   &bodies[i].v[1],
                   &bodies[i].v[2]) != 7) {
            fprintf(stderr, "Invalid input at body %d\n", i + 1);
            fclose(in);
            free(bodies);
            free(forces);
            return 1;
        }

        if (!isfinite(bodies[i].m) || bodies[i].m <= 0) {
            fprintf(stderr, "Invalid mass at body %d\n", i + 1);
            fclose(in);
            free(bodies);
            free(forces);
            return 1;
        }

        for (int k = 0; k < 3; k++) {
            if (!isfinite(bodies[i].r[k]) ||
                !isfinite(bodies[i].v[k])) {
                fprintf(stderr, "Invalid data at body %d\n", i + 1);
                fclose(in);
                free(bodies);
                free(forces);
                return 1;
            }
        }
    }

    fclose(in);

    threads = malloc((size_t)num_threads * sizeof(pthread_t));
    data = malloc((size_t)num_threads * sizeof(ThreadData));
    local_forces = calloc((size_t)num_threads, sizeof(double *));

    if (!threads || !data || !local_forces) {
        fprintf(stderr, "Memory allocation failed\n");
        free(threads);
        free(data);
        free(local_forces);
        free(bodies);
        free(forces);
        return 1;
    }

    for (int k = 0; k < num_threads; k++) {
        local_forces[k] = calloc((size_t)n * 3, sizeof(double));
        if (!local_forces[k]) {
            fprintf(stderr, "Memory allocation failed\n");
            for (int j = 0; j < k; j++)
                free(local_forces[j]);
            free(local_forces);
            free(threads);
            free(data);
            free(bodies);
            free(forces);
            return 1;
        }
    }

    FILE *out = fopen("trajectory.csv", "w");
    if (!out) {
        perror("Cannot create trajectory.csv");
        for (int k = 0; k < num_threads; k++)
            free(local_forces[k]);
        free(local_forces);
        free(threads);
        free(data);
        free(bodies);
        free(forces);
        return 1;
    }

    fprintf(out, "t");
    for (int i = 0; i < n; i++)
        fprintf(out, ",x%d,y%d,z%d", i + 1, i + 1, i + 1);
    fprintf(out, "\n");

    double t = 0.0;
    int step = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        fprintf(out, "%.10g", t);
        for (int i = 0; i < n; i++)
            fprintf(out, ",%.10g,%.10g,%.10g", bodies[i].r[0], bodies[i].r[1], bodies[i].r[2]);
        fprintf(out, "\n");

        if (t >= tend)
            break;

        double h = fmin(DT, tend - t); // шаг времени

        for (int k = 0; k < num_threads; k++)
            for (int j = 0; j < 3 * n; j++)
                local_forces[k][j] = 0.0;

        for (int k = 0; k < num_threads; k++) {
            data[k].id = k;
            data[k].start = (int)((long long)n * k / num_threads);
            data[k].end = (int)((long long)n * (k + 1) / num_threads);
            data[k].n = n;
            data[k].step = step;
            data[k].bodies = bodies;
            data[k].forces = local_forces[k];

            if (pthread_create(&threads[k], NULL, calculate_forces, &data[k]) != 0) {
                fprintf(stderr, "pthread_create failed\n");
                fclose(out);
                for (int j = 0; j < num_threads; j++)
                    free(local_forces[j]);
                free(local_forces);
                free(threads);
                free(data);
                free(bodies);
                free(forces);
                return 1;
            }
        }

        for (int k = 0; k < num_threads; k++)
            pthread_join(threads[k], NULL);

        for (int i = 0; i < 3 * n; i++) {
            forces[i] = 0.0;
            for (int k = 0; k < num_threads; k++)
                forces[i] += local_forces[k][i];
        }
        // обновление скоростей и координат тел (эйлеров метод)
        for (int i = 0; i < n; i++) {
            for (int d = 0; d < 3; d++) {
                bodies[i].r[d] += bodies[i].v[d] * h;
                bodies[i].v[d] += forces[3 * i + d] / bodies[i].m * h;
            }
        }

        t += h;
        step++;
        if (t >= tend || tend - t < 1e-12 * fmax(1.0, tend))
            t = tend;
    }

    fclose(out);

    for (int k = 0; k < num_threads; k++)
        free(local_forces[k]);

    free(local_forces);
    free(threads);
    free(data);
    free(bodies);
    free(forces);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    fprintf(stderr, "Time spent: %.6g seconds\n", time_spent);

    return 0;
}
