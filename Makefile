CC ?= gcc
CFLAGS ?= -Wall -ggdb3
OPTIMIZATION ?= -O0
LDFLAGS_DYNAMIC ?= -lhiredis -lhiredis_ssl -levent -lssl -lcrypto

SRC_COMMON := common.c
SRC_PINGPONG := pingpong.c
SRC_PING := ping.c

OBJ_COMMON := $(SRC_COMMON:.c=.o)
OBJ_PINGPONG := $(SRC_PINGPONG:.c=.o) $(OBJ_COMMON)
OBJ_PING := $(SRC_PING:.c=.o) $(OBJ_COMMON)

.PHONY: all clean pingpong ping release

all: pingpong ping

%.o: %.c
	$(CC) $(CFLAGS) $(OPTIMIZATION) -c $< -o $@

pingpong: $(OBJ_PINGPONG)
	$(CC) $(CFLAGS) $(OPTIMIZATION) $^ -o $@ $(LDFLAGS_DYNAMIC)

ping: $(OBJ_PING)
	$(CC) $(CFLAGS) $(OPTIMIZATION) $^ -o $@ $(LDFLAGS_DYNAMIC)

release:
	$(MAKE) OPTIMIZATION=-O2 all

clean:
	rm -f $(OBJ_PINGPONG) $(OBJ_PING) pingpong ping *.o

