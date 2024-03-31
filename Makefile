CC ?= gcc
CFLAGS ?= -Wall -ggdb3
OPTIMIZATION ?= -O0
LDFLAGS ?= -lhiredis -lhiredis_ssl -levent -lssl -lcrypto

SRC_COMMON := common.c
SRC_PINGPONG := pingpong.c
SRC_PING := ping.c

OBJ_COMMON := $(SRC_COMMON:.c=.o)
OBJ_PINGPONG := $(SRC_PINGPONG:.c=.o) $(OBJ_COMMON)
OBJ_PING := $(SRC_PING:.c=.o) $(OBJ_COMMON)

.PHONY: all clean pingpong ping release

# Detect operating system
UNAME_S := $(shell uname -s)

# If we are on macOS, adjust flags for OpenSSL and potentially other libraries
ifeq ($(UNAME_S),Darwin)
    # Assumes OpenSSL is installed via Homebrew. Adjust if necessary.
    OPENSSL_DIR := $(shell brew --prefix openssl)
    CFLAGS += -I$(OPENSSL_DIR)/include
    LDFLAGS += -L$(OPENSSL_DIR)/lib

    LIBEVENT_DIR := $(shell brew --prefix libevent)
    CFLAGS += -I$(LIBEVENT_DIR)/include
    LDFLAGS += -L$(LIBEVENT_DIR)/lib
endif

all: pingpong ping

%.o: %.c
	$(CC) $(CFLAGS) $(OPTIMIZATION) -c $< -o $@

pingpong: $(OBJ_PINGPONG)
	$(CC) $(CFLAGS) $(OPTIMIZATION) $^ -o $@ $(LDFLAGS)

ping: $(OBJ_PING)
	$(CC) $(CFLAGS) $(OPTIMIZATION) $^ -o $@ $(LDFLAGS)

release:
	$(MAKE) OPTIMIZATION=-O2 all

clean:
	rm -f $(OBJ_PINGPONG) $(OBJ_PING) pingpong ping *.o

