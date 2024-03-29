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

int main(int argc, char **argv) {
    size_t last_count, it;
    struct event_base *base;
    struct event *sigint;
    long long sst, st, et;
    options opts;
    int flags;

    redisAsyncContext *a;

    opts = parseOptions(argc, argv);

    if (opts.pub_chan == NULL) {
        panicAbort("pass --client a or --client b\n");
    }

    printf("host: %s, port: %d, tls: %s, subscribe: %s, publish: %s, message_len: %llu\n",
           opts.host, opts.port, opts.tls ? "true" : "false",
           opts.sub_chan, opts.pub_chan,
           opts.message_len);

    base = event_base_new();

    sigint = evsignal_new(base, SIGINT, sigintHandler, base);
    if (!sigint || event_add(sigint, NULL) < 0)
        panicAbort("Could not create/add a signal event!\n");

    a = getConnection(&opts);

    redisLibeventAttach(a, base);

    subscribeContext ctx = {.pub_chan = opts.pub_chan, .message_len = opts.message_len};
    setupConnection(a, opts.sub_chan, &ctx);

    sst = st = ustime();
    last_count = 0;
    it = 0;

    flags = opts.nonblock ? EVLOOP_NONBLOCK : EVLOOP_ONCE;

    while (!g_done) {
        event_base_loop(base, flags);
        if (g_messages - last_count >= 10000) {
            et = ustime();
            printStatus(++it, g_messages - last_count, et - st);
            last_count = g_messages;
            st = et;
        }
    }

    printStatus(++it, g_messages, ustime() - sst);

    redisAsyncDisconnect(a);

    event_free(sigint);
    event_base_free(base);
}
