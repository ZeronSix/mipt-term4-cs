#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>

#define TOTAL_SUBINTERVALS (1 * 2 * 3 * 5 * 6 * 7 * 8  * 300000L)
#define START 0.0
#define END 10.0

typedef struct WorkerArgs {
    double start;
    double end;
    long subintervals;
    double step;
    double result;
} WorkerArgs;

static inline __attribute__((always_inline)) double f(double x) {
    return (2 - x * x) / (4 + x);
}

static void *calculate_integral(void *data) {
    WorkerArgs *args = (WorkerArgs *)data;

    double value = (f(args->start) + f(args->end)) / 2;
    for (long i = 1; i < args->subintervals; i++) {
        value += f(args->start + args->step * i);
    }

    value *= args->step;
    args->result = value;

    return NULL;
}

static int parse_arg(const char *str, long *ptr);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: integral [worker count]\n");
        return EXIT_FAILURE;
    }

    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 0) {
        perror("sysconf cpu count");
        return EXIT_FAILURE;
    }
    long cache_linesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (cache_linesize < 0) {
        perror("sysconf cache");
        return EXIT_FAILURE;
    }

    long worker_count = 0;
    if (parse_arg(argv[1], &worker_count) < 0) {
        return EXIT_FAILURE;
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("pthread_attr_init");
        return EXIT_FAILURE;
    }

    pthread_t workers[worker_count];
    WorkerArgs *worker_args[worker_count];
    double step = (END - START) / TOTAL_SUBINTERVALS;
    for (long i = 0; i < worker_count; i++) {
        worker_args[i] = (WorkerArgs *)aligned_alloc(cache_linesize * 1024, sizeof(WorkerArgs));
        if (!worker_args[i]) {
            perror("aligned_alloc");
            return EXIT_FAILURE;
        }

        long subintervals = TOTAL_SUBINTERVALS / worker_count;
        if (i == worker_count - 1) {
            subintervals = TOTAL_SUBINTERVALS - i * TOTAL_SUBINTERVALS / worker_count;
        }

        worker_args[i]->start = START + i * step * subintervals;
        worker_args[i]->end = worker_args[i]->start + subintervals * step;
        worker_args[i]->subintervals = subintervals;
        worker_args[i]->step = step;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i % cpu_count, &cpuset);
        if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset)) {
            perror("pthread_attr_setaffinity_np");
            exit(EXIT_FAILURE);
        }

        if (pthread_create(&workers[i], &attr, calculate_integral, worker_args[i]) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    double value = 0;
    for (long i = 0; i < worker_count; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            perror("pthread_join");
            return EXIT_FAILURE;
        }
        value += worker_args[i]->result;
    }

    for (long i = 0; i < worker_count; i++) {
        free(worker_args[i]);
    }
    printf("%lg\n", value);

    return 0;
}

int parse_arg(const char *str, long *ptr) {
    long n = 0;
    char *endptr = NULL;
    errno = 0;
    n = strtol(str, &endptr, 10);

    if (errno != 0) {
        perror("strtol");
        return -1;
    }
    if (*endptr != '\0') {
        fprintf(stderr, "further chars!\n");
        return -1;
    }
    if (n < 1) {
        fprintf(stderr, "n < 1!\n");
        return -1;
    }

    *ptr = n;
    return 0;
}
