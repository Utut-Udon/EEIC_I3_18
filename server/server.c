/* server.c */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE   4096
#define REC_CMD    "rec -q -t raw -b 16 -c 1 -e s -r 48000 -"
#define PLAY_CMD   "play -q -t raw -b 16 -c 1 -e s -r 48000 -"

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

struct thread_arg {
    int sock;
};

void *send_thread(void *arg) {
    int s = ((struct thread_arg*)arg)->sock;
    FILE *pf = popen(REC_CMD, "r");
    if (!pf) die("popen(rec)");
    char buf[BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, pf)) > 0) {
        size_t sent = 0;
        while (sent < n) {
            ssize_t m = send(s, buf + sent, n - sent, 0);
            if (m <= 0) {
                goto out;
            }
            sent += m;
        }
    }
out:
    pclose(pf);
    shutdown(s, SHUT_WR);
    return NULL;
}

void *recv_thread(void *arg) {
    int s = ((struct thread_arg*)arg)->sock;
    FILE *pf = popen(PLAY_CMD, "w");
    if (!pf) die("popen(play)");
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = recv(s, buf, sizeof buf, 0)) > 0) {
        fwrite(buf, 1, (size_t)n, pf);
    }
    if (n < 0) die("recv");
    pclose(pf);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);

    int ss = socket(AF_INET, SOCK_STREAM, 0);
    if (ss < 0) die("socket");

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)atoi(argv[1]));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ss, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(ss, 1) < 0) die("listen");

    int s = accept(ss, NULL, NULL);
    if (s < 0) die("accept");
    close(ss);

    struct thread_arg targs = { .sock = s };
    pthread_t tx, rx;
    if (pthread_create(&tx, NULL, send_thread, &targs) != 0) die("pthread_create tx");
    if (pthread_create(&rx, NULL, recv_thread, &targs) != 0) die("pthread_create rx");

    pthread_join(tx, NULL);
    pthread_join(rx, NULL);
    close(s);
    return 0;
}
