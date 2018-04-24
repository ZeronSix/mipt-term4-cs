#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include "common.h"

static int handshake(int broadcastfd);
static double f(double x);
static double calculate(long start_subint, long subintervals);

int main(int argc, char *argv[])
{
    int retval = EXIT_SUCCESS;
    int fd = 0;

    int broadcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcastfd < 0)
    {
        perror("socket");
        retval = EXIT_FAILURE;
        goto RETURN;
    }

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
        retval = EXIT_FAILURE;
        goto CLOSE_BROADCASTFD;
    }

    while (1)
    {
        puts("Initiating handshake...");
        int fd = handshake(broadcastfd);
        if (fd < 0)
        {
            puts("Retrying to connect to client...");
            continue;
        }
        puts("Handshake established...");

        long argsbuf[2];
        ssize_t bytesread = read(fd, (char *)argsbuf, sizeof(argsbuf));
        if (bytesread < 0)
        {
            perror("read");
            close(fd);
            continue;
        }
        if (bytesread == 0)
        {
            puts("Lost connection!");
            close(fd);
            continue;
        }

        puts("Calculating...");
        double value = calculate(argsbuf[0], argsbuf[1]);
        printf("Calculated value: %lg\n\n", value);
        ssize_t bytessent = write(fd, &value, sizeof(value));
        if (bytessent < 0)
        {
            perror("bytessent");
        }
        else if (bytessent < sizeof(value))
        {
            fprintf(stderr, "write failure");
        }
        close(fd);
    }

    if (fd >= 0)
    {
        close(fd);
    }
CLOSE_BROADCASTFD:
    close(broadcastfd);
RETURN:
    return retval;
}

int handshake(int broadcastfd)
{
    char buf[1024];
    struct sockaddr_in from;
    socklen_t len = sizeof(from);

    ssize_t bytes_read = recvfrom(broadcastfd, buf, MAX_MSG_SIZE, 0,
                                  (struct sockaddr *)&from, &len);
    if (bytes_read < 0)
    {
        perror("recvfrom");
        return -1;
    }
    buf[bytes_read] = 0;
    if (strcmp(buf, MSG_BROADCAST) != 0)
    {
        puts("Failed handshake: wrong message. Dropping connection...");
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = CLIENT_DATA_RECEIVE_TIMEOUT;        // 30 Secs Timeout
    tv.tv_usec = 0;        // Not init'ing this can cause strange errors
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        close(sockfd);
        return -1;
    }

    from.sin_port = htons(PORT);
    puts("Connecting...");
    if (connect(sockfd, (struct sockaddr *)&from, sizeof(from)) < 0)
    {
        perror("connect");
        close(sockfd);
        return -1;
    }
    puts("Connection established...");

    ssize_t bytes_sent = send(sockfd, MSG_RESPONSE, sizeof(MSG_RESPONSE), 0);
    if (bytes_sent < 0)
    {
        perror("send");
        close(sockfd);
        return -1;
    }
    if (bytes_sent != sizeof(MSG_RESPONSE))
    {
        fprintf(stderr, "send wrong amount of bytes sent");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

double f(double x)
{
    return (2 - x * x) / (4 + x);
}

double calculate(long start_subint, long subintervals)
{
    double start = START + STEP * start_subint;
    double end = START + STEP * (start_subint + subintervals);
    printf("%lg %lg\n", start, end);

    double value = (f(start) + f(end)) / 2;
    for (long i = 1; i < subintervals; i++)
    {
        value += f(start + STEP * i);
    }
    value *= STEP;

    return value;
}
