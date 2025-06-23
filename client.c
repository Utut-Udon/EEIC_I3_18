// client.c
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
#define FRAME_SIZE       960      // 20ms @48kHz
#define FRAME_SIZE_10MS  480      // 10ms @48kHz
#define MAX_PACKET_BYTES 1276

static OpusEncoder  *opusEnc;
static OpusDecoder  *opusDec;
static DenoiseState *dnStateSend;
static DenoiseState *dnStateRecv;
static int            sockfd;
static volatile int   running = 1;
static struct sockaddr_in server_addr;
static FILE          *rec_pipe;
static FILE          *play_pipe;

static int capture_frame(opus_int16 *pcm, int size) {
    return fread(pcm, sizeof(opus_int16), size, rec_pipe);
}

void *sender_loop(void *arg) {
    opus_int16 pcm[FRAME_SIZE];
    float      fbuf[FRAME_SIZE_10MS];
    unsigned char packet[MAX_PACKET_BYTES];

    while (running) {
        if (capture_frame(pcm, FRAME_SIZE) != FRAME_SIZE)
            break;
        // RNNoise 前処理
        for (int off = 0; off < FRAME_SIZE; off += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[off + i];
            rnnoise_process_frame(dnStateSend, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[off + i] = (short)fbuf[i];
        }
        int bytes = opus_encode(opusEnc, pcm, FRAME_SIZE,
                                packet, MAX_PACKET_BYTES);
        if (bytes > 0) {
            sendto(sockfd, packet, bytes, 0,
                   (struct sockaddr*)&server_addr,
                   sizeof(server_addr));
        }
    }
    return NULL;
}

void *receiver_loop(void *arg) {
    unsigned char packet[MAX_PACKET_BYTES];
    opus_int16 pcm[FRAME_SIZE];
    float      fbuf[FRAME_SIZE_10MS];
    struct sockaddr_in src;
    socklen_t addrlen = sizeof(src);

    while (running) {
        int n = recvfrom(sockfd, packet, MAX_PACKET_BYTES, 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n <= 0) continue;
        int frame_size = opus_decode(opusDec, packet, n,
                                     pcm, FRAME_SIZE, 0);
        if (frame_size < 0) continue;
        // RNNoise ポストフィルタ
        for (int off = 0; off < frame_size; off += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[off + i];
            rnnoise_process_frame(dnStateRecv, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[off + i] = (short)fbuf[i];
        }
        fwrite(pcm, sizeof(opus_int16), frame_size, play_pipe);
        fflush(play_pipe);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }
    const char *ip   = argv[1];
    int          port = atoi(argv[2]);

    // RNNoise, Opus 初期化
    dnStateSend = rnnoise_create(NULL);
    dnStateRecv = rnnoise_create(NULL);
    int err;
    opusEnc = opus_encoder_create(SAMPLE_RATE, CHANNELS,
                                  OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) { fprintf(stderr, "opus_encoder_create: %s\n", opus_strerror(err)); return 1; }
    opus_decoder_destroy(opusDec); // 念のため
    opusDec = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK) { fprintf(stderr, "opus_decoder_create: %s\n", opus_strerror(err)); return 1; }
    opus_encoder_ctl(opusEnc, OPUS_SET_BITRATE(30000));

    // rec / play のパイプ
    rec_pipe  = popen("rec -t raw -b 16 -c 1 -e s -r 48000 -", "r");
    play_pipe = popen("play -t raw -b 16 -c 1 -e s -r 48000 -", "w");
    if (!rec_pipe || !play_pipe) {
        perror("popen");
        return 1;
    }

    // UDP ソケット
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    inet_aton(ip, &server_addr.sin_addr);

    // サーバーにダミーパケットを送り登録
    sendto(sockfd, "H", 1, 0,
           (struct sockaddr*)&server_addr, sizeof(server_addr));

    // スレッド起動
    pthread_t tid_snd, tid_rcv;
    pthread_create(&tid_snd, NULL, sender_loop, NULL);
    pthread_create(&tid_rcv, NULL, receiver_loop, NULL);

    // Enter で終了
    getchar();
    running = 0;
    pthread_join(tid_snd, NULL);
    pthread_join(tid_rcv, NULL);

    // 後始末
    pclose(rec_pipe);
    pclose(play_pipe);
    opus_encoder_destroy(opusEnc);
    opus_decoder_destroy(opusDec);
    rnnoise_destroy(dnStateSend);
    rnnoise_destroy(dnStateRecv);
    close(sockfd);
    return 0;
}
