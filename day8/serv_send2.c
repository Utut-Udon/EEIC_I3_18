/* serv_send2.c : 課題8.2 accept後にrecを起動し「今の音」だけ送信 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 4096
#define REC_CMD  "rec -q -t raw -b 16 -c 1 -e s -r 44100 -"

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
    /* --- socket/bind/listen (同じ) --- */
    int ss = socket(AF_INET, SOCK_STREAM, 0);
    if (ss == -1) die("socket");

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)atoi(argv[1]));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("bind");
    if (listen(ss, 10) == -1) die("listen");

    /* --- accept --- */
    int s = accept(ss, NULL, NULL);
    if (s == -1) die("accept");
    close(ss);

    /* --- rec を popen() で実行し stdout を読む --- */
    FILE *pf = popen(REC_CMD, "r");
    if (!pf) die("popen(rec)");

    char buf[BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, pf)) > 0) {
        size_t off = 0;
        while (off < n) {
            ssize_t m = send(s, buf + off, n - off, 0);
            if (m <= 0) { pclose(pf); die("send"); }
            off += (size_t)m;
        }
    }
    if (ferror(pf)) { pclose(pf); die("fread(rec)"); }

    pclose(pf);
    shutdown(s, SHUT_WR);
    close(s);
    return 0;
}
