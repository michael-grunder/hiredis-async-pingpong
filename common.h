#ifndef PINGPONG_COMMON_H
#define PINGPONG_COMMON_H

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <stdbool.h>

typedef enum subscribeType {
    SUBSCRIBE,
    MESSAGE,
} subscribeType;

typedef struct subscribePayload {
    subscribeType type;
    const char *channel;
    unsigned long long message_len;
    unsigned long long id;
} subscribePayload;

typedef struct options {
    const char *host;
    int port;
    bool tls;
    bool nonblock;
    char *sub_chan;
    char *pub_chan;
    unsigned long long message_len;
} options;

typedef struct subscribeContext {
    const char *pub_chan;
    unsigned long long message_len;
    unsigned long long id;
} subscribeContext;

#define panicAbort(fmt, ...) \
    do { \
        fprintf(stderr, "%s:%d - " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        exit(1); \
    } while (1)

long long ustime(void);

double opsPerSec(long long elapsed_usec, long long ops);
subscribePayload getSubscribePayload(redisReply *reply);
void setupConnection(redisAsyncContext *ac, char *sub_chan, subscribeContext *ctx);
void publishCb(redisAsyncContext *c, void *r, void *privdata);
void subscribeCb(redisAsyncContext *c, void *r, void *privdata);
void printStatus(size_t iteration, size_t num_ops, long long elapsed_usec);

redisAsyncContext *getConnection(options *opts);
options parseOptions(int argc, char **argv);

#endif
