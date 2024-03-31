## Hiredis "ping pong" demonstrator

This is a small test application which uses async hiredis to "ping pong" messages back and forth between two different pubsub channels.

```bash
Usage: ./pingpong [options]
Options:
  --host <hostname>       Set the Redis server hostname (default: localhost)
  --port <port>           Set the Redis server port (default: 6380, or 16380 if --tls is specified)
  --tls                   Enable TLS connection
  --nonblock              Use non-blocking mode
  --client <a|b>          Set the client to either 'a' or 'b', determining pub/sub channels
  --message-len <length>  Set the length of the message to be published (max: 10MB)
  -h, --help              Print this message.```
```


## The `pingpong` binary does both subscribing and publishing
```bash
$ ./pingpong --tls --message-len 1024
```

## The `ping` binary needs to be run twice, once for each client.
```bash
$ ./ping --tls --client a --message-len 1024
$ ./ping --tls --client b --message-len 1024
```


## Dockerfile
This repo contains a dockerfile you can use to actually run the tests.  It creates all the keys we need to start a TLS secured redis-server which will run when you start the container.
