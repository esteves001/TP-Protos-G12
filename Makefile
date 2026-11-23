# Makefile - Proxy SOCKSv5 (TP Final, Protocolos de Comunicación, ITBA)
#
# Targets:
#   make          -> bin/socks5d y bin/client
#   make test     -> compila y corre tests de libcheck (test/*_test.c)
#   make debug    -> -O0 -g -fsanitize=address,undefined
#   make clean

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -pedantic -Wall -Wextra -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS ?=
LDLIBS  ?= -lpthread

SRC_DIR    := src
CLIENT_DIR := client
TEST_DIR   := test
BIN_DIR    := bin
OBJ_DIR    := $(BIN_DIR)/obj
TEST_BIN   := $(BIN_DIR)/test

SERVER := $(BIN_DIR)/socks5d
CLIENT := $(BIN_DIR)/client

# --- servidor ---------------------------------------------------------------
SRV_SRCS := $(wildcard $(SRC_DIR)/*.c)
SRV_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRV_SRCS))
# objetos linkeables por tests y cliente (todo menos main.o)
LIB_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(SRV_OBJS))

# --- cliente ----------------------------------------------------------------
CLI_SRCS := $(wildcard $(CLIENT_DIR)/*.c)
CLI_OBJS := $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/client_%.o,$(CLI_SRCS))
# módulos de src/ que el cliente puede reusar (I/O bloqueante permitida solo acá)
CLI_SHARED := $(OBJ_DIR)/netutils.o $(OBJ_DIR)/buffer.o

# --- tests ------------------------------------------------------------------
TEST_SRCS := $(wildcard $(TEST_DIR)/*_test.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(TEST_BIN)/%,$(TEST_SRCS))
# tests que incluyen el .c directamente (#include "buffer.c") -> no linkear el .o
TESTS_SELF_CONTAINED := buffer_test selector_test
CHECK_CFLAGS := $(shell pkg-config --cflags check 2>/dev/null)
CHECK_LIBS   := $(shell pkg-config --libs check 2>/dev/null || echo -lcheck)

.PHONY: all clean test debug dirs

all: dirs $(SERVER) $(CLIENT)

dirs:
	@mkdir -p $(OBJ_DIR) $(TEST_BIN)

$(SERVER): $(SRV_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(CLIENT): $(CLI_OBJS) $(CLI_SHARED)
	@if [ -z "$(CLI_OBJS)" ]; then echo "client/: sin fuentes todavía, se omite $(CLIENT)"; else \
	    $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS); fi

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c -o $@ $<

$(OBJ_DIR)/client_%.o: $(CLIENT_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(CLIENT_DIR) -c -o $@ $<

$(TEST_BIN)/%: $(TEST_DIR)/%.c $(LIB_OBJS) | dirs
	$(CC) $(CFLAGS) $(CHECK_CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $< \
	    $(if $(filter $*,$(TESTS_SELF_CONTAINED)),,$(LIB_OBJS)) $(LDFLAGS) $(CHECK_LIBS) $(LDLIBS)

test: $(TEST_BINS)
	@rc=0; for t in $(TEST_BINS); do echo "== $$t"; $$t || rc=1; done; exit $$rc

debug: CFLAGS := -O0 -g -fsanitize=address,undefined $(filter-out -O2,$(CFLAGS))
debug: LDFLAGS += -fsanitize=address,undefined
debug: all

clean:
	rm -rf $(BIN_DIR)
