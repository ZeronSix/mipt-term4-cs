#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <linux/if_link.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include "common.h"

static int bind_socket(int fd);
static int broadcast(int broadcastfd);
static int server_handshake(int sockfd);
static void *thread_routine(void *data);

typedef struct ThreadArgs
{
    long index;
    long total_threads;
    int sockfd;
    int broadcastfd;
    double result;
} ThreadArgs;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    int retval = EXIT_SUCCESS;
    int broadcastfd = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: client [worker_count]\n");
        return EXIT_FAILURE;
    }

    long n = 0;
    if (parse_arg(argv[1], &n) < 0)
    {
        return EXIT_FAILURE;
    }

    pthread_t threads[n];
    ThreadArgs threadargs[n];

    broadcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastfd < 0)
    {
        perror("socket");
        retval = EXIT_FAILURE;
        goto RETURN;
    }

    int on = 1;
    if (setsockopt(broadcastfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
    {
        perror("setsockopt");
        retval = EXIT_FAILURE;
        goto CLOSE_BROADCASTFD;
    }

    if (setsockopt(broadcastfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        perror("setsockopt");
        retval = EXIT_FAILURE;
        goto CLOSE_BROADCASTFD;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        retval = EXIT_FAILURE;
        goto CLOSE_BROADCASTFD;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        perror("setsockopt");
        retval = EXIT_FAILURE;
        goto CLOSE_SOCKFD;
    }

    /*
    struct timeval tv;
    tv.tv_sec = CLIENT_DATA_RECEIVE_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        retval = EXIT_FAILURE;
        goto CLOSE_SOCKFD;
    } */

    if (setfd_nonblock(sockfd) < 0)
    {
        retval = EXIT_FAILURE;
        goto CLOSE_SOCKFD;
    }

    if (bind_socket(sockfd) < 0)
    {
        retval = EXIT_FAILURE;
        goto CLOSE_SOCKFD;
    }

    puts("Initial broadcast...");

    if (broadcast(broadcastfd) < 0)
    {
        retval = EXIT_FAILURE;
        goto CLOSE_SOCKFD;
    }

    for (long i = 0; i < n; i++)
    {
        threadargs[i].index = i;
        threadargs[i].total_threads = n;
        threadargs[i].sockfd = sockfd;
        threadargs[i].broadcastfd = broadcastfd;

        if (pthread_create(&threads[i], NULL, thread_routine, &threadargs[i]) != 0)
        {
            perror("pthread_create");
            goto CLOSE_SOCKFD;
        }
    }

    double value = 0;
    for (long i = 0; i < n; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("pthread_join");
            goto CLOSE_SOCKFD;
        }
        value += threadargs[i].result;
    }

    printf("\nResult value: %lg\n", value);

CLOSE_SOCKFD:
    close(sockfd);
CLOSE_BROADCASTFD:
    close(broadcastfd);
RETURN:
    return retval;
}

int bind_socket(int fd)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    if (listen(fd, MAX_SERVERS) < 0)
    {
        perror("listen");
        return -1;
    }

    return 0;
}

int broadcast(int broadcastfd)
{
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
    int retval = 0;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        retval = -1;
        goto RETURN;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (!(ifa->ifa_flags & IFF_BROADCAST))
            continue;

        printf("Broadcasting on %-8s...\n", ifa->ifa_name);

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_broadaddr;
        addr->sin_port = htons(PORT);
        int result = sendto(broadcastfd, MSG_BROADCAST, sizeof(MSG_BROADCAST), 0,
                            (struct sockaddr *)addr, sizeof(*addr));
        if (result < 0)
        {
            perror("sendto");
            retval = -1;
            goto FREEIFADDRS;
        }
    }

FREEIFADDRS:
    freeifaddrs(ifaddr);
RETURN:
    return retval;
}

int server_handshake(int sockfd)
{
    char buf[MAX_MSG_SIZE + 1] = {0};
    int fd = accept(sockfd, NULL, NULL);
    ssize_t bytes_read = read(fd, buf, MAX_MSG_SIZE);
    if (bytes_read < 0)
    {
        perror("read");
        return -1;
    }
    else if(bytes_read == 0)
    {
        puts("Lost connection to server...");
        close(fd);
        return -1;
    }

    int keepalive = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0)
    {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    int keepcnt = 5, keepidle = 5, keepintvl = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(int));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(int));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(int));

    return (strcmp(MSG_RESPONSE, buf) == 0) ? fd : -1;
}

static void *thread_routine(void *data)
{
    // TODO: fix type
    int64_t retval = EXIT_SUCCESS;
    ThreadArgs *args = (ThreadArgs *)data;
    int sockfd = args->sockfd;
    int broadcastfd = args->broadcastfd;

    long start_subint = args->index * (TOTAL_SUBINTERVALS / args->total_threads);
    long subintervals = 0;
    if (args->index != args->total_threads - 1)
    {
        subintervals = TOTAL_SUBINTERVALS / args->total_threads;
    }
    else
    {
        subintervals = TOTAL_SUBINTERVALS - (TOTAL_SUBINTERVALS / args->total_threads) *
                       (args->total_threads - 1);
    }

    int serverfd = 0;
    // TODO: move to another function?
    while (1)
    {
        // TODO: do something with unlocking
        pthread_mutex_lock(&mutex);
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);

        puts("Accepting connection...");

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = CLIENT_BROADCAST_TIMEOUT_US;

        int selres = select(sockfd + 1, &readset,
                            NULL, NULL, &tv);
        if (selres < 0)
        {
            perror("select");
            retval = EXIT_FAILURE;
            pthread_mutex_unlock(&mutex);
            goto RETURN;
        }
        else if(selres == 0)
        {
            puts("Select timeout! repeating broadcast...");
            if (broadcast(broadcastfd) < 0)
            {
                pthread_mutex_unlock(&mutex);
                retval = EXIT_FAILURE;
                goto RETURN;
            }

            pthread_mutex_unlock(&mutex);
            continue;
        }

        puts("Connecting to server...");
        serverfd = server_handshake(sockfd);
        pthread_mutex_unlock(&mutex);
        if (serverfd >= 0)
        {
            puts("Server connected");
        }
        else
        {
            puts("Server handshake failed...");
        }

        long buffer[] = {start_subint, subintervals};
        if (write(serverfd, (char *)buffer, sizeof(buffer)) < sizeof(buffer))
        {
            perror("write");
            close(serverfd);
            continue;
        }

        double value;
        ssize_t bytesread = read(serverfd, (char *)&value, sizeof(value));
        if (bytesread < 0)
        {
            perror("read");
            close(serverfd);
            exit(EXIT_FAILURE);
            continue;
        }
        else if (bytesread == 0)
        {
            puts("Lost connection!");
            close(serverfd);
            exit(EXIT_FAILURE);
            continue;
        }

        args->result = value;
        printf("Received: %lg\n", value);
        close(serverfd);
        break;
    }

RETURN:
    return (void *)retval;
}
