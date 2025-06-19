#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

static void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int	main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

	int	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s == -1)
		die("socket");

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)atoi(argv[2]));
	if (inet_aton(argv[1], &addr.sin_addr) == 0)
	{
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(s);
        return EXIT_FAILURE;
	}

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		die("connect");

	char	buf[BUF_SIZE];
	ssize_t n;
	while (1)
	{
		ssize_t n = recv(s, buf, sizeof(buf), 0);
		if (n == 0)
			break;
		write(STDOUT_FILENO, buf, n);
	}
	if (n == -1)
		die("recv");
	close(s);
}



	// int	s = socket(PF_INET, SOCK_STREAM, 0);
	// struct sockaddr_in addr;
	// addr.sin_family = AF_INET;
	// addr.sin_addr.s_addr = inet_addr("192.168.1.100");
	// addr.sin_port = htons(50000);
	// int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	// n = recv(s, data, N, 0);