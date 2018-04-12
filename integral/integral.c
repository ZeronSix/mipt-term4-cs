#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include "cpuinfo.h"

#define TOTAL_SUBINTERVALS (1 * 2 * 3 * 5 * 6 * 7 * 8  * 300000L)
#define START 0.0
#define END 10.0
#define STEP ((END - START) / TOTAL_SUBINTERVALS)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct WorkerArgs {
    double start;
    double end;
    long subintervals;
    double result;
} WorkerArgs;

static inline __attribute__((always_inline)) double f(double x) {
    return (2 - x * x) / (4 + x);
}

static void *calculate_integral(void *data) {
    WorkerArgs *args = (WorkerArgs *)data;

    double start = args->start;
    double end = args->end;
    long subintervals = args->subintervals;

    double value = (f(start) + f(end)) / 2;
    for (long i = 1; i < subintervals; i++) {
        value += f(start + STEP * i);
    }

    value *= STEP;
    args->result = value;

    return NULL;
}

static void *fake_thread(void *data) {
    for (;;) {
    }

    return NULL;
}

static int parse_arg(const char *str, long *ptr);

static void spawn(pthread_t *t,
                  pthread_attr_t *attr,
                  WorkerArgs *args,
                  double start,
                  double end,
                  int cpu,
                  size_t subintervals) {
    //printf("%d %lg %lg %ld\n", cpu, start, end, subintervals);
    args->start = start;
    args->end = end;
    args->subintervals = subintervals;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset)) {
        perror("pthread_attr_setaffinity_np");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(t, attr, calculate_integral, args) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    size_t physical_cores = 0;
    size_t cores_used = 0;
    long worker_count = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: integral [worker count]\n");
        return EXIT_FAILURE;
    }

    if (parse_arg(argv[1], &worker_count) < 0) {
        return EXIT_FAILURE;
    }

    cpuinfo_parse();
    physical_cores = cpuinfo_getphysicalcores();
    cores_used = MIN(physical_cores, worker_count);

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("pthread_attr_init");
        return EXIT_FAILURE;
    }

    pthread_t workers[worker_count];
    WorkerArgs worker_args[worker_count];
    size_t free_subintervals = TOTAL_SUBINTERVALS;
    size_t subintervals_per_core = TOTAL_SUBINTERVALS / cores_used;
    long free_workers = worker_count;
    long workers_per_core = worker_count / cores_used;
    long n = 0;
    double start = START;
    double end = END;

    for (int core = 0; core < cores_used; core++) {
        size_t subintervals_thiscore = subintervals_per_core;
        long workers_thiscore = workers_per_core;
        if (core == cores_used - 1) {
            subintervals_thiscore = free_subintervals;
            workers_thiscore = free_workers;
        }
        free_subintervals -= subintervals_thiscore;
        free_workers -= workers_thiscore;

        size_t logical_cnt = MIN(cpuinfo_getlogicalcores(core),
                                 workers_thiscore);
        long workers_per_logical = workers_thiscore / logical_cnt;
        size_t subintervals_per_logical = subintervals_thiscore / logical_cnt;

        for (long curlogical = 0; curlogical < logical_cnt; curlogical++) {
            long workers_log = workers_per_logical;
            size_t subintervals_log = subintervals_per_logical;
            if (curlogical == logical_cnt - 1) {
                workers_log = workers_thiscore;
                subintervals_log = subintervals_thiscore;
            }
            workers_thiscore -= workers_log;
            subintervals_thiscore -= subintervals_log;

            size_t subintervals_per_worker = subintervals_log / workers_log;
            for (long i = 0; i < workers_log; i++) {
                size_t subintervals = subintervals_per_worker;
                if (i == workers_log - 1) {
                    subintervals = subintervals_log;
                }
                subintervals_log -= subintervals;
                end = start + subintervals * STEP;
                spawn(&workers[n], &attr, &worker_args[n],
                      start,
                      end,
                      cpuinfo_getlogicalcoreid(core, curlogical),
                      subintervals);
                start = end;
                n++;
            }
        }
    }

    for (long i = worker_count; i < physical_cores; i++) {
        int cpu = cpuinfo_getlogicalcoreid(i, 0);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        if (pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset)) {
            perror("pthread_attr_setaffinity_np");
            exit(EXIT_FAILURE);
        }

        pthread_t t;
        if (pthread_create(&t, &attr, fake_thread, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    double value = 0;
    for (long i = 0; i < worker_count; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            perror("pthread_join");
            return EXIT_FAILURE;
        }
        value += worker_args[i].result;
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
