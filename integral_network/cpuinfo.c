#include "cpuinfo.h"
#include <stdlib.h>
#include <stdio.h>

#define MAX_CORES 128

typedef struct PhysicalCore
{
    int logical_cores[MAX_CORES];
    size_t logical_count;
    size_t cur_logical;
} PhysicalCore;

static PhysicalCore cores[MAX_CORES] = {0};
static size_t logical_cores = 0;
static size_t physical_cores = 0;

void cpuinfo_parse(void)
{
    FILE *f = popen("cat /proc/cpuinfo | egrep 'core id|physical id' | tr -d '\\n' | sed s/physical/\\\\nphysical/g | grep -v ^$ | sort | uniq | wc -l", "r");
    if (!f)
    {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    if (fscanf(f, "%lu", &physical_cores) != 1)
    {
        fprintf(stderr, "fscanf error\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);

    f = popen("grep -c processor /proc/cpuinfo", "r");
    if (!f)
    {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    if (fscanf(f, "%lu", &logical_cores) != 1)
    {
        fprintf(stderr, "fscanf error\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);

    f = popen("lscpu -p=cpu,core | grep -v ^#", "r");
    int cpu = 0;
    int coreid = 0;
    while (fscanf(f, "%d,%d", &cpu, &coreid) == 2)
    {
        cores[coreid].logical_cores[cores[coreid].logical_count] = cpu;
        cores[coreid].logical_count++;
    }
}

size_t cpuinfo_getphysicalcores(void)
{
    return physical_cores;
}

size_t cpuinfo_getlogicalcores(int coreid)
{
    return cores[coreid].logical_count;
}

size_t cpuinfo_getlogicalcoreid(int coreid, int n)
{
    return cores[coreid].logical_cores[n];
}
