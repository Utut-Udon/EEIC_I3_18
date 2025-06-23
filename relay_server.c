// relay_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_PACKET_BYTES 1276
#define MAX_CLIENTS       2

static int sockaddr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr
        && a->sin_port        == b->sin_port;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port        = htons(port);
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); close(sockfd); return 1;
    }

    struct sockaddr_in peers[MAX_CLIENTS];
    int peer_count = 0;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    unsigned char buf[MAX_PACKET_BYTES];

    printf("Relay server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in src;
        int n = recvfrom(sockfd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }
        // 新規クライアント登録
        int known = 0;
        for (int i = 0; i < peer_count; i++) {
            if (sockaddr_eq(&peers[i], &src)) {
                known = 1;
                break;
            }
        }
        if (!known && peer_count < MAX_CLIENTS) {
            peers[peer_count++] = src;
            printf("New client: %s:%d\n",
                   inet_ntoa(src.sin_addr), ntohs(src.sin_port));
        }
        // 中継：送信元以外へ転送
        for (int i = 0; i < peer_count; i++) {
            if (!sockaddr_eq(&peers[i], &src)) {
                sendto(sockfd, buf, n, 0,
                       (struct sockaddr*)&peers[i], addrlen);
            }
        }
    }

    // never reached
    close(sockfd);
    return 0;
}
