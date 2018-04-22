#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "common.h"
#include "list.h"

static int parse_arg(const char *str, long *ptr);
static int setfd_nonblock(int fd);
static int setfd_block(int fd);
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
        fprintf(stderr, "Usage: client [worker count]\n");
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
    for (long i = 0; i < n; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
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
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    int result = sendto(broadcastfd, MSG_BROADCAST, sizeof(MSG_BROADCAST), 0,
                        (struct sockaddr *)&addr, sizeof(addr));
    if (result < 0)
    {
        perror("sendto");
    }

    return result;
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
    while (1)
    {
        // TODO: do something with unlocking
        pthread_mutex_lock(&mutex);
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);

        puts("Accepting connection...");

        struct timeval tv;
        tv.tv_sec = CLIENT_BROADCAST_TIMEOUT;
        tv.tv_usec = 0;

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
            continue;
        }
        else if (bytesread == 0)
        {
            puts("Lost connection!");
            close(serverfd);
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
