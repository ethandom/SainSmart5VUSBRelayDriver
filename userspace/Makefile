# userspace/Makefile - build relayctl CLI

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -g
INCLUDES:= -Iinclude

TOOLS_DIR := tools
BIN       := $(TOOLS_DIR)/relayctl

SRCS := $(TOOLS_DIR)/relayctl.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TOOLS_DIR)/%.o: $(TOOLS_DIR)/%.c include/usbrelay.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	$(RM) $(OBJS) $(BIN)
