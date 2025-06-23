# Makefile for relay_server and client

CC          := gcc
OPUS_PREFIX := /opt/homebrew/opt/opus
RNNOISE_LIB := /usr/local/lib

CFLAGS   := -std=c11 -O2 -Wall \
             -I$(OPUS_PREFIX)/include \
             -I/usr/local/include
LDFLAGS  := -pthread \
            -L$(OPUS_PREFIX)/lib \
            -L$(RNNOISE_LIB)
LDLIBS   := -lopus -lrnnoise -lm

TARGETS  := relay_server client

.PHONY: all clean re

all: $(TARGETS)

relay_server: relay_server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

client: client.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGETS)

re: clean all
