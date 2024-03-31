#include "common.h"
#include <stdlib.h>
#include <strings.h>
#include <openssl/ssl.h>
#include <hiredis/hiredis_ssl.h>

unsigned long long g_messages = 0;

typedef struct publishMessage {
    unsigned long long id;
    size_t len;
    char data[];
} publishMessage;

static publishMessage *
createPublishMessage(unsigned long long len, unsigned long long id) {
    publishMessage *msg;

    msg = malloc(sizeof(*msg) + len);
    if (msg == NULL)
        panicAbort("Failed to allocate memory for message\n");

    msg->id = id;
    msg->len = len;

    memset(&msg->data, 'Z', len);

    return msg;
}

static void freePublishMessage(publishMessage *msg) {
    free(msg);
}

static size_t publishMessageSize(publishMessage *msg) {
    return sizeof(*msg) + msg->len;
}

long long ustime(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

double opsPerSec(long long elapsed_usec, long long ops) {
    return ops / ((double)elapsed_usec / 1000000);
}

static void readMessageInfo(unsigned long long *id, unsigned long long *len,
                            redisReply *reply)
{
    if (reply->len < sizeof(publishMessage))
        panicAbort("Payload is too short!");

    memcpy(id, reply->str, sizeof(*id));
    memcpy(len, reply->str + sizeof id, sizeof(*len));
}

subscribePayload getSubscribePayload(redisReply *reply) {
    if (reply == NULL) {
        panicAbort("reply is NULL\n");
    }

    if (reply->type != REDIS_REPLY_PUSH)
        panicAbort("reply->type is not REDIS_REPLY_PUSH\n");

    if (reply->elements != 3)
        panicAbort("reply->elements is not 3\n");

    if (reply->element[0]->type != REDIS_REPLY_STRING ||
        reply->element[1]->type != REDIS_REPLY_STRING)
    {
        panicAbort("reply element 0 or 1 is not a string\n");
    }

    if (!strncasecmp(reply->element[0]->str, "subscribe", 9)) {
        if (reply->element[2]->type != REDIS_REPLY_INTEGER)
            panicAbort("Expected element 2 to be an integer\n");

        return (subscribePayload) {
            SUBSCRIBE,
            reply->element[1]->str,
            reply->element[2]->integer,
            0,
        };
    } else if (!strncasecmp(reply->element[0]->str, "message", 7)) {
        if (reply->element[2]->type != REDIS_REPLY_STRING)
            panicAbort("expected element 2 to be a string\n");

        unsigned long long id, message_len;
        readMessageInfo(&id, &message_len, reply->element[2]);

        return (subscribePayload) {
            MESSAGE,
            reply->element[1]->str,
            message_len,
            id,
        };
    } else {
        panicAbort("Unsupported message type: %s\n", reply->element[0]->str);
    }
}

void publishCb(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;

    (void)privdata;
    (void)c;

    if (c->err) {
        panicAbort("Redis Error: %s\n", c->errstr);
    } else if (reply && reply->type == REDIS_REPLY_ERROR) {
        panicAbort("Redis Error: %s\n", reply->str);
    }
}

void subscribeCb(redisAsyncContext *c, void *r, void *privdata) {
    subscribeContext *ctx = privdata;
    subscribePayload payload;
    redisReply *reply = r;
    publishMessage *msg;

    if (reply == NULL) {
        if (c->err)
            panicAbort("Redis Error: %s\n", c->errstr);
        return;
    }

    payload = getSubscribePayload(reply);

    if (payload.type == SUBSCRIBE) {
        printf("Subscribed to '%s' (%llu subscribers)\n", payload.channel,
               payload.message_len);
    }

    msg = createPublishMessage(ctx->message_len, payload.id + 1);
    ctx->id = payload.id;

    ++g_messages;

    redisAsyncCommand(c, publishCb, NULL, "PUBLISH %s %b", ctx->pub_chan, msg,
                      publishMessageSize(msg));

    freePublishMessage(msg);
}

void printUsage(const char *program) {
    fprintf(stderr, "Usage: %s [options]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --host <hostname>       Set the Redis server hostname (default: localhost)\n");
    fprintf(stderr, "  --port <port>           Set the Redis server port (default: 6380, or 16380 if --tls is specified)\n");
    fprintf(stderr, "  --tls                   Enable TLS connection\n");
    fprintf(stderr, "  --nonblock              Use non-blocking mode\n");
    fprintf(stderr, "  --client <a|b>          Set the client to either 'a' or 'b', determining pub/sub channels\n");
    fprintf(stderr, "  --message-len <length>  Set the length of the message to be published (max: 10MB)\n");
    fprintf(stderr, "  -h, --help              Print this message.");
    fprintf(stderr, "\n");

    exit(0);
}

options parseOptions(int argc, char **argv) {
    options opts = {0};
    const char *program;

    program = argv[0];

    argc--; argv++;

    while (argc) {
        if (!strcasecmp(*argv, "--host") && argc > 1) {
            argc--; argv++;
            opts.host = *argv;
        } else if (!strcasecmp(*argv, "--port") && argc > 1) {
            argc--; argv++;
            opts.port = atoi(*argv);
        } else if (!strcasecmp(*argv, "--tls")) {
            opts.tls = true;
        } else if (!strcasecmp(*argv, "--nonblock")) {
            opts.nonblock = true;
        } else if (!strcasecmp(*argv, "--client") && argc > 1) {
            argc--; argv++;
            if (!strcasecmp(*argv, "a")) {
                opts.sub_chan = "client_b";
                opts.pub_chan = "client_a";
            } else if (!strcasecmp(*argv, "b")) {
                opts.sub_chan = "client_a";
                opts.pub_chan = "client_b";
            } else {
                panicAbort("Unknown channel: %s\n", *argv);
            }
        } else if (!strcasecmp(*argv, "--message-len") && argc > 1) {
            argc--; argv++;
            opts.message_len = strtoull(*argv, NULL, 10);
            if (opts.message_len > 1024 * 1024 * 10) {
                panicAbort("Message length %llu too large\n", opts.message_len);
            }
        } else if (!strcasecmp(*argv, "--help") || !strcasecmp(*argv, "-h")) {
            printUsage(program);
        } else {
            fprintf(stderr, "Warning: Unknown option: %s\n", *argv);
        }

        argc--; argv++;
    }

    if (!opts.host)
        opts.host = "localhost";

    if (opts.tls) {
        if (!opts.port) {
            opts.port = 16380;
        }
    } else if (!opts.port) {
        opts.port = 6380;
    }

    return opts;
}

redisAsyncContext *getConnection(options *opts) {
    redisAsyncContext *ac;
    SSL_CTX *ssl_ctx;
    SSL *ssl;

    ac = redisAsyncConnect(opts->host, opts->port);
    if (ac == NULL || ac->err) {
        panicAbort("Can't create async context (%s)\n",
                   ac ? ac->errstr : "unknown error");
    }

    if (opts->tls == false)
        return ac;

    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl_ctx)
        panicAbort("Failed to create SSL_CTX\n");

    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

    ssl = SSL_new(ssl_ctx);
    if (!ssl)
        panicAbort("Failed to create SSL\n");

    if (redisInitiateSSL(&ac->c, ssl) != REDIS_OK)
        panicAbort("Failed to initialize redisAsyncContext with SSL\n");

    return ac;
}

void printStatus(size_t iteration, size_t num_ops, long long elapsed_usec) {
    double ops_per_sec = opsPerSec(elapsed_usec, num_ops);
    printf("[%zu] %zu messages in %lld usec (%2.2f/sec)\n",
           iteration, num_ops, elapsed_usec, ops_per_sec);
}

void
setupConnection(redisAsyncContext *ac, char *sub_chan, subscribeContext *ctx)
{
    if (redisAsyncCommand(ac, NULL, NULL, "HELLO %s", "3") != REDIS_OK) {
        panicAbort("Failed to send HELLO 3\n");
    }

    if (redisAsyncCommand(ac, subscribeCb, ctx, "SUBSCRIBE %s",
                          sub_chan) != REDIS_OK)
    {
        panicAbort("Failed to subscribe to channel %s\n", sub_chan);
    }

    redisAsyncSetPushCallback(ac, NULL);
}
