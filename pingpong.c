#include <hiredis/hiredis.h>
#include <hiredis/hiredis_ssl.h>
#include <hiredis/adapters/libevent.h>
#include <event2/event.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

#include "common.h"

static volatile sig_atomic_t g_done;
extern unsigned long long g_messages;

void sigintHandler(evutil_socket_t fd, short event, void *arg) {
    struct event_base *base = arg;
    struct timeval delay = {0, 0};

    (void)event;
    (void)fd;

    printf("Caught SIGINT\n");

    g_done = 1;

    event_base_loopexit(base, &delay);
}

void execLibevent(options *opts) {
    long long sst, st, et, last_count, it;
    redisAsyncContext *a, *b;
    struct event_base *base;
    struct event *sigint;
    int flags;

    base = event_base_new();

    sigint = evsignal_new(base, SIGINT, sigintHandler, base);
    if (!sigint || event_add(sigint, NULL) < 0)
        panicAbort("Could not create/add a signal event!\n");

    a = getConnection(opts);
    if (redisLibeventAttach(a, base) != REDIS_OK)
        panicAbort("Could not attach libevent to redis context!\n");

    b = getConnection(opts);
    if (redisLibeventAttach(b, base) != REDIS_OK)
        panicAbort("Could not attach libevent to redis context!\n");

    subscribeContext ctx_a = {.pub_chan = "client_a", .message_len = opts->message_len};
    setupConnection(a, "client_b", &ctx_a);

    subscribeContext ctx_b = {.pub_chan = "client_b", .message_len = opts->message_len};
    setupConnection(b, "client_a", &ctx_b);

    sst = st = ustime();
    last_count = 0;
    it = 0;

    flags = opts->nonblock ? EVLOOP_NONBLOCK : EVLOOP_ONCE;

    while (!g_done) {
        event_base_loop(base, flags);

        if (g_messages - last_count >= 10000) {
            et = ustime();
            printStatus(++it, g_messages - last_count, et - st);
            last_count = g_messages;
            st = et;
        }
    }

    printStatus(++it, ctx_a.id + ctx_b.id, ustime() - sst);

    redisAsyncDisconnect(a);
    redisAsyncDisconnect(b);

    event_free(sigint);
    event_base_free(base);
}

int main(int argc, char **argv) {
    options opts;

    opts = parseOptions(argc, argv);

    printf("host: %s, port: %d, tls: %s, message len: %llu\n",
           opts.host, opts.port, opts.tls ? "true" : "false",
           opts.message_len);

    execLibevent(&opts);
}
