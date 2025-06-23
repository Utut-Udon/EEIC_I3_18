#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include </opt/homebrew/opt/opus/include/opus/opus.h>
#include <rnnoise.h>

#define SAMPLE_RATE      48000
#define CHANNELS         1
#define FRAME_SIZE       960     // 20ms @48kHz
#define FRAME_SIZE_10MS  480     // 10ms @48kHz
#define MAX_PACKET_BYTES 1276

static OpusDecoder *opusDec;
static DenoiseState *dnStateRecv;
static int sockfd;
static volatile int running = 1;
static struct sockaddr_in client_addr;
static socklen_t addr_len;
static FILE *play_pipe;

void *receiver_loop(void *arg) {
    unsigned char packet[MAX_PACKET_BYTES];
    opus_int16 pcm[FRAME_SIZE];
    float fbuf[FRAME_SIZE_10MS];

    while (running) {
        int recv_bytes = recvfrom(sockfd, packet, MAX_PACKET_BYTES, 0,
                                  (struct sockaddr *)&client_addr, &addr_len);
        if (recv_bytes <= 0) continue;
        int frame_size = opus_decode(opusDec, packet, recv_bytes, pcm, FRAME_SIZE, 0);
        if (frame_size < 0) continue;
        // RNNoiseポストフィルタ
        for (int offset = 0; offset < frame_size; offset += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[offset + i];
            rnnoise_process_frame(dnStateRecv, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[offset + i] = (short)fbuf[i];
        }
        // play コマンドへの出力
        fwrite(pcm, sizeof(opus_int16), frame_size, play_pipe);
        fflush(play_pipe);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);

    // RNNoiseデコーダ
    dnStateRecv = rnnoise_create(NULL);
    int err;
    opusDec = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "Failed to create Opus decoder: %s\n", opus_strerror(err));
        return 1;
    }

    // play プロセス起動
    play_pipe = popen("play -t raw -b 16 -c 1 -e s -r 48000 -", "w");
    if (!play_pipe) {
        perror("popen play");
        return 1;
    }

    // UDPソケット
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    addr_len = sizeof(client_addr);
    char buf[1];
    recvfrom(sockfd, buf, 1, 0, (struct sockaddr *)&client_addr, &addr_len);
    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    pthread_t tid_recv;
    pthread_create(&tid_recv, NULL, receiver_loop, NULL);

    getchar();
    running = 0;
    pthread_join(tid_recv, NULL);

    pclose(play_pipe);
    opus_decoder_destroy(opusDec);
    rnnoise_destroy(dnStateRecv);
    close(sockfd);
    return 0;
}