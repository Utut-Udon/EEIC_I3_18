#include "server_lib.h"

#define MAX_PACKET_BYTES 1276
#define MAX_CLIENTS       10

static volatile int running = 0;
static int sockfd = -1;
static pthread_t thread_id;

static struct sockaddr_in peers[MAX_CLIENTS];
static int peer_count = 0;
static pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

// sockaddr_in 同士を IP+port で比較
static int sockaddr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr
        && a->sin_port        == b->sin_port;
}

// relay ループ本体（pthread エントリ）
static void *relay_loop(void *arg) {
    uint16_t port = *(uint16_t*)arg;
    free(arg);

    struct sockaddr_in serv = {0};
    serv.sin_family      = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port        = htons(port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }
    if (bind(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("bind");
        close(sockfd);
        return NULL;
    }

    running = 1;
    printf("Relay server listening on port %u...\n", port);

    while (running) {
        unsigned char buf[MAX_PACKET_BYTES];
        struct sockaddr_in src;
        socklen_t addrlen = sizeof(src);
        int n = recvfrom(sockfd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        // 新規クライアント登録
        pthread_mutex_lock(&peers_mutex);
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
        // peers コピー
        struct sockaddr_in copy[MAX_CLIENTS];
        int cnt = peer_count;
        memcpy(copy, peers, cnt * sizeof(peers[0]));
        pthread_mutex_unlock(&peers_mutex);

        // 中継送信
        for (int i = 0; i < cnt; i++) {
            if (!sockaddr_eq(&copy[i], &src)) {
                sendto(sockfd, buf, n, 0,
                       (struct sockaddr*)&copy[i], addrlen);
            }
        }
    }

    close(sockfd);
    sockfd = -1;
    printf("Relay server stopped.\n");
    return NULL;
}

void start_server(uint16_t port) {
    if (running) return;  // すでに起動中なら無視
    uint16_t *argp = malloc(sizeof(uint16_t));
    *argp = port;
    pthread_create(&thread_id, NULL, relay_loop, argp);
}

void stop_server(void) {
    if (!running) return;
    running = 0;
    // ソケットを unblock させるためダミー送信
    if (sockfd >= 0) {
        struct sockaddr_in dummy = { .sin_family = AF_INET };
        dummy.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        dummy.sin_port = 0;
        sendto(sockfd, "", 1, 0,
               (struct sockaddr*)&dummy, sizeof(dummy));
    }
    pthread_join(thread_id, NULL);
}
