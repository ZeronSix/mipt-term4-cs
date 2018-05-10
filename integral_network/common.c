#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int parse_arg(const char *str, long *ptr)
{
    long n = 0;
    char *endptr = NULL;
    errno = 0;
    n = strtol(str, &endptr, 10);

    if (errno != 0)
    {
        perror("strtol");
        return -1;
    }
    if (*endptr != '\0')
    {
        fprintf(stderr, "further chars!\n");
        return -1;
    }
    if (n < 1)
    {
        fprintf(stderr, "n < 1!\n");
        return -1;
    }

    *ptr = n;
    return 0;
}

int setfd_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        perror("fcntl");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int setfd_block(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        perror("fcntl");
        return -1;
    }
    flags &= !O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}
