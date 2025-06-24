#include "relay_lib.h"

#define MAX_PACKET_BYTES 1276
#define MAX_CLIENTS       10

static volatile int relay_running = 0;
static int sockfd_relay = -1;
static struct sockaddr_in peers[MAX_CLIENTS];
static int peer_count = 0;

static int sockaddr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr
        && a->sin_port        == b->sin_port;
}

void *relay_thread(void *arg) {
    uint16_t port = *(uint16_t*)arg;
    free(arg);

    struct sockaddr_in servaddr = {0};
    sockfd_relay = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_relay < 0) return NULL;

    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port        = htons(port);
    if (bind(sockfd_relay, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        close(sockfd_relay);
        return NULL;
    }

    relay_running = 1;
    while (relay_running) {
        unsigned char buf[MAX_PACKET_BYTES];
        struct sockaddr_in src;
        socklen_t addrlen = sizeof(src);
        int n = recvfrom(sockfd_relay, buf, sizeof(buf), 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n < 0) continue;

        // Register new client
        int known = 0;
        for (int i = 0; i < peer_count; i++) {
            if (sockaddr_eq(&peers[i], &src)) { known = 1; break; }
        }
        if (!known && peer_count < MAX_CLIENTS) {
            peers[peer_count++] = src;
        }

        // Relay to others
        for (int i = 0; i < peer_count; i++) {
            if (!sockaddr_eq(&peers[i], &src)) {
                sendto(sockfd_relay, buf, n, 0,
                       (struct sockaddr*)&peers[i], addrlen);
            }
        }
    }

    close(sockfd_relay);
    sockfd_relay = -1;
    return NULL;
}

void stop_relay(void) {
    relay_running = 0;
    if (sockfd_relay >= 0) close(sockfd_relay);
}
