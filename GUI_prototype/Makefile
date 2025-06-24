CC := gcc
PKG_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 opus rnnoise)
PKG_LIBS   := $(shell pkg-config --libs   gtk+-3.0 opus rnnoise)
CFLAGS := -Wall -g $(PKG_CFLAGS) -pthread
LDFLAGS := $(PKG_LIBS) -pthread

SRCS := server_lib.c client_lib.c app.c
OBJS := $(SRCS:.c=.o)
TARGET := app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

re: clean all