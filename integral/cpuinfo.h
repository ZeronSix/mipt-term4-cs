#ifndef CPUINFO_H
#define CPUINFO_H
#include <stdlib.h>

void cpuinfo_parse(void);
int cpuinfo_getnextcpu(int *coreid);
size_t cpuinfo_getphysicalcores(void);
size_t cpuinfo_getlogicalcores(int coreid);
size_t cpuinfo_getlogicalcoreid(int coreid, int n);

#endif /* ifndef CPUINFO_H */
