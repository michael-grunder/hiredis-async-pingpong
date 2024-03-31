FROM debian:latest as build0

RUN apt-get -y update && apt-get -y install \
    build-essential \
    git libssl-dev \
    redis-server \
    libevent-dev \
    neovim

FROM build0 as build1

RUN git clone https://github.com/redis/hiredis && cd hiredis && \
    git checkout v1.2.0 && USE_SSL=1 make install -j$(nproc) && \
    ldconfig

FROM build1 as build2

COPY . /root/hiredis-test

RUN cd /root/hiredis-test && make release -j$(nproc) && \
    ./config-tls-redis.sh /root/hiredis-test/redis

CMD ["redis-server", "/root/hiredis-test/redis/redis.conf"]
