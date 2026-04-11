# Makefile – rpiimager
#
# Build:
#   make                   (native)
#   make CC=aarch64-linux-gnu-gcc  (cross-compile for ARM64)
#
# Dependencies: SDL2, SDL2_ttf, libcurl, pthreads

CC      ?= gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 \
           $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) \
           -lSDL2_ttf -lcurl -lpthread -lm

TARGET  := rpiimager

SRCS := \
    main.c    \
    ui.c      \
    input.c   \
    oslist.c  \
    flash.c   \
    net.c     \
    json.c

OBJS := $(SRCS:.c=.o)

# Headers that should trigger a full rebuild if changed
HDRS := types.h ui.h input.h oslist.h flash.h net.h json.h jsmn.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
