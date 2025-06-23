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
#define FRAME_SIZE       960
#define FRAME_SIZE_10MS  480
#define MAX_PACKET_BYTES 1276

static OpusEncoder *opusEnc;
static DenoiseState *dnStateSend;
static int sockfd;
static volatile int running = 1;
static struct sockaddr_in server_addr;
static FILE *rec_pipe;

int capture_frame(opus_int16 *pcm, int size) {
    return fread(pcm, sizeof(opus_int16), size, rec_pipe);
}

void *sender_loop(void *arg) {
    opus_int16 pcm[FRAME_SIZE];
    float fbuf[FRAME_SIZE_10MS];
    unsigned char packet[MAX_PACKET_BYTES];

    while (running) {
        int got = capture_frame(pcm, FRAME_SIZE);
        if (got != FRAME_SIZE) break;
        // RNNoise前処理
        for (int offset = 0; offset < FRAME_SIZE; offset += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[offset + i];
            rnnoise_process_frame(dnStateSend, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[offset + i] = (short)fbuf[i];
        }
        int bytes = opus_encode(opusEnc, pcm, FRAME_SIZE, packet, MAX_PACKET_BYTES);
        if (bytes < 0) continue;
        sendto(sockfd, packet, bytes, 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <dummy>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    dnStateSend = rnnoise_create(NULL);
    int err;
    opusEnc = opus_encoder_create(SAMPLE_RATE, CHANNELS,
                                  OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) return 1;
    opus_encoder_ctl(opusEnc, OPUS_SET_BITRATE(30000));

    // rec プロセス起動
    rec_pipe = popen("rec -t raw -b 16 -c 1 -e s -r 48000 -", "r");
    if (!rec_pipe) {
        perror("popen rec");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ip, &server_addr.sin_addr);

    // ハンドシェイク
    sendto(sockfd, "H", 1, 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));

    pthread_t tid_send;
    pthread_create(&tid_send, NULL, sender_loop, NULL);

    getchar();
    running = 0;
    pthread_join(tid_send, NULL);

    pclose(rec_pipe);
    opus_encoder_destroy(opusEnc);
    rnnoise_destroy(dnStateSend);
    close(sockfd);
    return 0;
}
