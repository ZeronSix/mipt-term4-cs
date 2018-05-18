#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include "common.h"
#include "cpuinfo.h"

typedef struct ThreadArgs
{
    int broadcastfd;
} ThreadArgs;

static int bind_broadcastsock(int broadcastfd);
static int spawn_threads(pthread_t *threads, ThreadArgs *threadargs,
                          pthread_attr_t *attr, long n,
                          size_t physical_cores, size_t cores_used);
static void server_routine(int broadcastfd);
static void *thread_routine(void *data);
static int handshake(int broadcastfd);
static double f(double x);
static double calculate(long start_subint, long subintervals);
static void *fake_thread(void *data);

int main(int argc, char *argv[])
{
    int retval = EXIT_SUCCESS;
    size_t physical_cores = 0;
    size_t cores_used = 0;
    long n = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: server [worker_count]\n");
        return EXIT_FAILURE;
    }
    if (parse_arg(argv[1], &n) < 0)
    {
        return EXIT_FAILURE;
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
    {
        perror("pthread_attr_init");
        return EXIT_FAILURE;
    }

    cpuinfo_parse();
    physical_cores = cpuinfo_getphysicalcores();
    cores_used = MIN(physical_cores, n);

    pthread_t threads[n];
    ThreadArgs threadargs[n];

    int broadcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastfd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (bind_broadcastsock(broadcastfd) < 0)
    {
        goto CLOSE_BROADCASTFD;
    }

    for (long i = 0; i < n; i++)
    {
        threadargs[i].broadcastfd = broadcastfd;
    }

    if (spawn_threads(threads, threadargs, &attr, n, physical_cores, cores_used) < 0)
    {
        goto CLOSE_BROADCASTFD;
    }

    for (long i = 0; i < n; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("pthread_join");
            goto CLOSE_BROADCASTFD;
        }
    }

CLOSE_BROADCASTFD:
    close(broadcastfd);
    return retval;
}

int bind_broadcastsock(int broadcastfd)
{
    struct sockaddr_in broadcastaddr;
    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_port = htons(PORT);
    broadcastaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int result = 0;
    if ((result = bind(broadcastfd,
                       (struct sockaddr *)&broadcastaddr,
                       sizeof(broadcastaddr)) < 0))
    {
        perror("bind");
        return -1;
    }

    return 0;
}

// TODO: refactor
int spawn_threads(pthread_t *threads, ThreadArgs *threadargs,
                   pthread_attr_t *attr, long n,
                   size_t physical_cores, size_t cores_used)
{
    long free_workers = n;
    long workers_per_core = n / cores_used;

    for (int core = 0; core < cores_used; core++)
    {
        long workers_thiscore = workers_per_core;
        if (core == cores_used - 1)
        {
            workers_thiscore = free_workers;
        }
        free_workers -= workers_thiscore;

        size_t logical_cnt = MIN(cpuinfo_getlogicalcores(core),
                                 workers_thiscore);
        long workers_per_logical = workers_thiscore / logical_cnt;

        for (long curlogical = 0; curlogical < logical_cnt; curlogical++)
        {
            long workers_log = workers_per_logical;
            if (curlogical == logical_cnt - 1)
            {
                workers_log = workers_thiscore;
            }
            workers_thiscore -= workers_log;

            for (long i = 0; i < workers_log; i++)
            {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(cpuinfo_getlogicalcoreid(core, curlogical), &cpuset);
                if (pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset))
                {
                    perror("pthread_attr_setaffinity_np");
                    return -1;
                }

                if (pthread_create(&threads[i], attr, thread_routine, &threadargs[i]) != 0)
                {
                    perror("pthread_create");
                    return -1;
                }
            }
        }
    }

    // kill TurboBoost
    for (long i = n; i < physical_cores; i++)
    {
        int cpu = cpuinfo_getlogicalcoreid(i, 0);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        if (pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset))
        {
            perror("pthread_attr_setaffinity_np");
            return -1;
        }

        pthread_t t;
        if (pthread_create(&t, attr, fake_thread, NULL) != 0)
        {
            perror("pthread_create");
            return -1;
        }
    }

    return 0;
}

void server_routine(int broadcastfd)
{
    puts("Initiating handshake...");
    int fd = handshake(broadcastfd);
    if (fd < 0)
    {
        puts("Retrying to connect to client...");
        return;
    }
    puts("Handshake established...");

    long argsbuf[2];
    ssize_t bytesread = read(fd, (char *)argsbuf, sizeof(argsbuf));
    if (bytesread < 0)
    {
        perror("read");
        goto CLOSE_FD;
    }
    else if (bytesread == 0)
    {
        puts("Lost connection!");
        goto CLOSE_FD;
    }

    puts("Calculating...");
    double value = calculate(argsbuf[0], argsbuf[1]);
    printf("Calculated value: %lg\n\n", value);
    ssize_t bytessent = write(fd, &value, sizeof(value));
    if (bytessent < 0)
    {
        perror("bytessent");
        exit(EXIT_FAILURE);
        goto CLOSE_FD;
    }
    else if (bytessent < sizeof(value))
    {
        fprintf(stderr, "write failure");
        exit(EXIT_FAILURE);
        goto CLOSE_FD;
    }

CLOSE_FD:
    close(fd);
}

void *thread_routine(void *data)
{
    puts("Thread spawned");
    ThreadArgs *args = (ThreadArgs *)data;
    int broadcastfd = args->broadcastfd;

    while (1)
    {
        server_routine(broadcastfd);
    }

    return NULL;
}

int handshake(int broadcastfd)
{
    int sockfd = -1;
    char buf[1024] = {0};

    struct sockaddr_in from;
    socklen_t len = sizeof(from);
    ssize_t bytes_read = recvfrom(broadcastfd, buf, MAX_MSG_SIZE, 0,
                                  (struct sockaddr *)&from, &len);
    if (bytes_read < 0)
    {
        perror("recvfrom");
        goto RETURN;
    }
    buf[bytes_read] = 0;
    if (strcmp(buf, MSG_BROADCAST) != 0)
    {
        puts("Failed handshake: wrong message. Dropping connection...");
        goto RETURN;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        goto RETURN;
    }

    struct timeval tv;
    tv.tv_sec = CLIENT_DATA_RECEIVE_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        goto CLOSE_SOCKFD;
    }

    int keepalive = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0)
    {
        perror("setsockopt");
        goto CLOSE_SOCKFD;
    }

    int keepcnt = 5, keepidle = 5, keepintvl = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));

    puts("Connecting...");
    from.sin_port = htons(PORT);
    if (connect(sockfd, (struct sockaddr *)&from, sizeof(from)) < 0)
    {
        perror("connect");
        goto CLOSE_SOCKFD;
    }
    puts("Connection established...");

    ssize_t bytes_sent = send(sockfd, MSG_RESPONSE, sizeof(MSG_RESPONSE), 0);
    if (bytes_sent < 0)
    {
        perror("send");
        goto CLOSE_SOCKFD;
    }
    if (bytes_sent != sizeof(MSG_RESPONSE))
    {
        fprintf(stderr, "send wrong amount of bytes sent");
        goto CLOSE_SOCKFD;
    }

    return sockfd;

    // only for errors
CLOSE_SOCKFD:
    close(sockfd);
RETURN:
    return -1;
}

double f(double x)
{
    return (2 - x * x) / (4 + x);
}

double calculate(long start_subint, long subintervals)
{
    double start = START + STEP * start_subint;
    double end = START + STEP * (start_subint + subintervals);
    printf("Start: %lg, end: %lg\n", start, end);

    double value = (f(start) + f(end)) / 2;
    for (long i = 1; i < subintervals; i++)
    {
        value += f(start + STEP * i);
    }
    value *= STEP;

    return value;
}

void *fake_thread(void *data)
{
    for (;;) {}

    return NULL;
}
