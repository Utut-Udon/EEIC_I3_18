# Makefile for GUI VoIP app using GTK+, Opus, and RNNoise

CC := gcc
# Use pkg-config to fetch compiler and linker flags
PKG_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 opus rnnoise)
PKG_LIBS    := $(shell pkg-config --libs   gtk+-3.0 opus rnnoise)

# Our compilation flags
CFLAGS  := -Wall -g $(PKG_CFLAGS) -pthread
LDFLAGS := -pthread $(PKG_LIBS)

SRCS   := relay_lib.c client_lib.c app.c
OBJS   := $(SRCS:.c=.o)
TARGET := app

.PHONY: all clean

all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

re: clean all
