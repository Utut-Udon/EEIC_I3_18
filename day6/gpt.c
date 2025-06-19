#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 100000

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

/* 送信完了までループ */
static void xsend(int sock, const char *buf, size_t len)
{
    while (len) {
        ssize_t m = send(sock, buf, len, 0);
        if (m == -1) die("send");
        buf += m;
        len -= (size_t)m;
    }
}

/* 出力完了までループ */
static void xwrite(const char *buf, size_t len)
{
    while (len) {
        ssize_t m = write(STDOUT_FILENO, buf, len);
        if (m == -1) die("write");
        buf += m;
        len -= (size_t)m;
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* ---- connect ---- */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) die("socket");

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)atoi(argv[2]));
    if (inet_aton(argv[1], &addr.sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(sock);
        return EXIT_FAILURE;
    }
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("connect");

    /* ---- stdin → 送信 ---- */
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
        xsend(sock, buf, (size_t)n);
    if (n == -1) die("read");

    /* 送信終了を通知 */
    if (shutdown(sock, SHUT_WR) == -1) die("shutdown");

    /* ---- 受信 → stdout ---- */
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
        xwrite(buf, (size_t)n);
    if (n == -1) die("recv");

    close(sock);
    return EXIT_SUCCESS;
}
