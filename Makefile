CC = gcc
PKG_CONFIG = pkg-config
CFLAGS = -Wall -O2 $(shell $(PKG_CONFIG) --cflags rnnoise opus)
LDFLAGS = $(shell $(PKG_CONFIG) --libs rnnoise opus)

TARGETS = voip_server voip_client

all: $(TARGETS)

voip_server: server.c
	$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

voip_client: client.c
	$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)
