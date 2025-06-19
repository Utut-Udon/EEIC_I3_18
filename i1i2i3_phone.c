#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

#define N 256

typedef struct
{
    int sock;
    FILE *pipe;
} thread_arg_t;

void *send_thread(void *arg)
{
    thread_arg_t *targ = (thread_arg_t *)arg;
    unsigned char data[N];
    ssize_t n;
    while ((n = fread(data, 1, sizeof(data), targ->pipe)) > 0)
    {
        ssize_t sent = 0;
        while (sent < n)
        {
            ssize_t m = write(targ->sock, data + sent, n - sent);
            if (m < 0)
            {
                perror("write");
                pthread_exit(NULL);
            }
            sent += m;
        }
    }
    pthread_exit(NULL);
}

void *recv_thread(void *arg)
{
    thread_arg_t *targ = (thread_arg_t *)arg;
    unsigned char data[N];
    ssize_t n;
    while ((n = read(targ->sock, data, sizeof(data))) > 0)
    {
        ssize_t written = 0;
        while (written < n)
        {
            ssize_t m = fwrite(data + written, 1, n - written, targ->pipe);
            if (m <= 0)
            {
                perror("fwrite");
                pthread_exit(NULL);
            }
            written += m;
        }
        fflush(targ->pipe);
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int s;
    if (argc == 2)
    {
        int ss = socket(PF_INET, SOCK_STREAM, 0);
        if (ss == -1)
        {
            perror("socket");
            exit(1);
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(atoi(argv[1]));
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            perror("bind");
            close(ss);
            exit(1);
        }
        if (listen(ss, 10) == -1)
        {
            perror("listen");
            close(ss);
            exit(1);
        }
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        s = accept(ss, (struct sockaddr *)&client_addr, &len);
        if (s == -1)
        {
            perror("accept");
            close(ss);
            exit(1);
        }
        close(ss);
    }
    else if (argc == 3)
    {
        s = socket(PF_INET, SOCK_STREAM, 0);
        if (s == -1)
        {
            perror("socket");
            exit(1);
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(atoi(argv[2]));
        if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1)
        {
            fprintf(stderr, "Invalid address\n");
            exit(1);
        }
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            perror("connect");
            close(s);
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s <Port> (server)\n       %s <ServerAddr> <Port> (client)\n", argv[0], argv[0]);
        exit(1);
    }

    FILE *rec_pipe = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");
    if (!rec_pipe)
    {
        perror("popen rec");
        close(s);
        exit(1);
    }

    FILE *play_pipe = popen("play -t raw -b 16 -c 1 -e s -r 44100 -", "w");
    if (!play_pipe)
    {
        perror("popen play");
        close(s);
        pclose(rec_pipe);
        exit(1);
    }

    pthread_t tid_send, tid_recv;
    thread_arg_t send_arg = {s, rec_pipe};
    thread_arg_t recv_arg = {s, play_pipe};

    pthread_create(&tid_send, NULL, send_thread, &send_arg);
    pthread_create(&tid_recv, NULL, recv_thread, &recv_arg);

    pthread_join(tid_send, NULL);
    pthread_cancel(tid_recv);
    pthread_join(tid_recv, NULL);

    pclose(rec_pipe);
    pclose(play_pipe);
    close(s);
    return 0;
}
