#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int	main(int argc, char **argv)
{
	int	s = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("192.168.1.100");
	addr.sin_port = htons(50000);
	int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
	n = recv(s, data, N, 0);
}

