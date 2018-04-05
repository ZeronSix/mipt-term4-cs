#include "cpuinfo.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX_CORES 128

typedef struct PhysicalCore {
    int logical_cores[MAX_CORES];
    size_t logical_count;
    size_t cur_logical;
} PhysicalCore;

static PhysicalCore cores[MAX_CORES] = {0};
static size_t cur_physical = 0;
static size_t core_count = 0;

void cpuinfo_parse(void) {
    FILE *f = popen("awk -F': ' '$1 ~ /^(core id)/||/(^processor)/"
                    "{print $2}' /proc/cpuinfo", "r");
    if (!f) {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    int processor = 0;
    int coreid = 0;
    while (fscanf(f, "%d %d", &processor, &coreid) == 2) {
        if (coreid + 1 > core_count) {
            core_count = coreid + 1;
        }
        if (core_count > MAX_CORES) {
            fprintf(stderr, "Too much logical cores!\n");
            exit(EXIT_FAILURE);
        }

        cores[coreid].logical_cores[cores[coreid].logical_count] = processor;
        cores[coreid].logical_count++;
    }

    fclose(f);
}

int cpuinfo_getnextcpu(void) {
    size_t cur_logical = cores[cur_physical].cur_logical;
    int processor = cores[cur_physical].logical_cores[cur_logical];
    cores[cur_physical].cur_logical = (cur_logical + 1) % cores[cur_physical].logical_count;
    cur_physical = (cur_physical + 1) % core_count;

    // printf("The next processor is %d\n", processor);
    return processor;
}
