#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1000

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 3)
	{
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1)
        die("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    if (inet_aton(argv[1], &servaddr.sin_addr) == 0)
	{
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(s);
        return EXIT_FAILURE;
    }

    {
        unsigned char dummy = 0x00;
        if (sendto(s, &dummy, 1, 0,
                   (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            die("sendto");
        }
    }

    unsigned char buf[BUF_SIZE];
    ssize_t n;
    while (1) {
        n = recvfrom(s, buf, BUF_SIZE, 0, NULL, NULL);
        if (n == -1)
            die("recvfrom");

        if (n == BUF_SIZE) {
            int i;
            for (i = 0; i < BUF_SIZE; ++i) {
                if (buf[i] != 0x01)
                    break;
            }
            if (i == BUF_SIZE)
                break;
        }

        if (write(STDOUT_FILENO, buf, n) == -1)
            die("write");
    }

    close(s);
    return 0;
}
