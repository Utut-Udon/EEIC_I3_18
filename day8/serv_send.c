/* serv_send.c : 課題8.1 基本形  stdin→クライアントへ送信のみ            */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 4096

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    /* ---------- ステップ1: socket ---------- */
    int ss = socket(AF_INET, SOCK_STREAM, 0);
    if (ss == -1) die("socket");

    /* ---------- ステップ2: bind ---------- */
    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)atoi(argv[1]));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("bind");

    /* ---------- ステップ3: listen ---------- */
    if (listen(ss, 10) == -1) die("listen");

    /* ---------- ステップ4: accept ---------- */
    int s = accept(ss, NULL, NULL);
    if (s == -1) die("accept");
    close(ss);

    /* ---------- ステップ5: stdin→socket ---------- */
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof buf)) > 0) {
        ssize_t off = 0;
        while (off < n) {            /* 部分送信対策 */
            ssize_t m = send(s, buf + off, n - off, 0);
            if (m <= 0) die("send");
            off += m;
        }
    }
    if (n == -1) die("read(stdin)");
    shutdown(s, SHUT_WR);            /* 送信終了を通知 (任意)              */

    close(s);
    return 0;
}
