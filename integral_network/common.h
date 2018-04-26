#ifndef COMMON_H
#define COMMON_H

#define PORT 1234
#define TOTAL_SUBINTERVALS (5000 * 300000L)
#define START 0.0
#define END 10.0
#define STEP ((END - START) / TOTAL_SUBINTERVALS)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define CLIENT_BROADCAST_TIMEOUT 1
#define CLIENT_DATA_RECEIVE_TIMEOUT 60

typedef struct Data
{
    double start;
    double end;
    int subintervals;
} Data;

#define PERROR_AND_EXIT(str) \
    do \
    { \
        perror(str); \
        exit(EXIT_FAILURE); \
    } while (0)
#define PERROR_AND_GOTO(str, label) \
    do \
    { \
        perror(str); \
        retval = EXIT_FAILURE; \
        goto label; \
    } while (0)

#define MAX_MSG_SIZE 1024
#define MAX_SERVERS 1024

static const char MSG_BROADCAST[] = "HI";
static const char MSG_RESPONSE[] = "OH HI";

int parse_arg(const char *str, long *ptr);
int setfd_nonblock(int fd);
int setfd_block(int fd);

#endif /* ifndef COMMON_H */
