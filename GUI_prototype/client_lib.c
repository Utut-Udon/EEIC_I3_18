// client_lib.c

#include "client_lib.h"

#define SAMPLE_RATE      48000   // サンプリング周波数
#define CHANNELS         1       // モノラル
#define FRAME_SIZE       960     // 20ms @ 48kHz
#define FRAME_SIZE_10MS  480     // 10ms 単位
#define MAX_PACKET_BYTES 1276    // UDP パケット最大サイズ

static volatile int   client_running = 0;  // 送受信ループ制御フラグ
static int            sockfd_client   = -1;
static OpusEncoder   *opusEnc;
static OpusDecoder   *opusDec;
static DenoiseState  *dnSend, *dnRecv;
static FILE          *rec_pipe, *play_pipe;

// マイクから PCM フレームを読み込む
static int capture_frame(opus_int16 *pcm, int size) {
    return fread(pcm, sizeof(opus_int16), size, rec_pipe);
}

// 音声送信ループ
static void *sender_loop(void *arg) {
    struct sockaddr_in *srv = arg;
    opus_int16   pcm[FRAME_SIZE];
    float        fbuf[FRAME_SIZE_10MS];
    unsigned char packet[MAX_PACKET_BYTES];
    socklen_t addrlen = sizeof(*srv);

    while (client_running) {
        if (capture_frame(pcm, FRAME_SIZE) != FRAME_SIZE)
            break;

        // RNNoise 前処理（10msごと）
        for (int off = 0; off < FRAME_SIZE; off += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[off + i];
            rnnoise_process_frame(dnSend, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[off + i] = (short)fbuf[i];
        }

        // Opus エンコード & 送信
        int bytes = opus_encode(opusEnc, pcm, FRAME_SIZE,
                                packet, MAX_PACKET_BYTES);
        if (bytes > 0) {
            sendto(sockfd_client, packet, bytes, 0,
                   (struct sockaddr*)srv, addrlen);
        }
    }
    return NULL;
}

// 音声受信ループ（デバッグログ付き）
static void *receiver_loop(void *arg) {
    (void)arg;
    unsigned char packet[MAX_PACKET_BYTES];
    opus_int16    pcm[FRAME_SIZE];
    float         fbuf[FRAME_SIZE_10MS];
    struct sockaddr_in src;
    socklen_t addrlen = sizeof(src);

    while (client_running) {
        int n = recvfrom(sockfd_client, packet, MAX_PACKET_BYTES, 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n <= 0) {
            fprintf(stderr, "[client recv] n=%d\n", n);
            continue;
        }
        fprintf(stderr, "[client recv] %d bytes from %s:%d\n",
                n, inet_ntoa(src.sin_addr), ntohs(src.sin_port));

        // Opus デコード
        int frame_size = opus_decode(opusDec, packet, n,
                                     pcm, FRAME_SIZE, 0);
        if (frame_size < 0) {
            fprintf(stderr, "[client decode] error %d\n", frame_size);
            continue;
        }

        // RNNoise ポスト処理
        for (int off = 0; off < frame_size; off += FRAME_SIZE_10MS) {
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                fbuf[i] = pcm[off + i];
            rnnoise_process_frame(dnRecv, fbuf, fbuf);
            for (int i = 0; i < FRAME_SIZE_10MS; i++)
                pcm[off + i] = (short)fbuf[i];
        }

        // PCM を再生パイプへ書き込み
        fwrite(pcm, sizeof(opus_int16), frame_size, play_pipe);
        fflush(play_pipe);
    }
    return NULL;
}

// クライアントスレッドエントリポイント
void *client_thread(void *arg) {
    char *conn_str = arg;
    char ip[64];
    uint16_t port;
    sscanf(conn_str, "%63[^:]:%hu", ip, &port);
    free(conn_str);

    struct sockaddr_in srv = {0};
    sockfd_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_client < 0) return NULL;
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(port);
    inet_aton(ip, &srv.sin_addr);

    // RNNoise, Opus 初期化
    dnSend = rnnoise_create(NULL);
    dnRecv = rnnoise_create(NULL);
    int err;
    opusEnc = opus_encoder_create(SAMPLE_RATE, CHANNELS,
                                  OPUS_APPLICATION_VOIP, &err);
    opusDec = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);

    // rec/play パイプ起動
    rec_pipe  = popen("rec -t raw -b 16 -c 1 -e s -r 48000 -", "r");
    play_pipe = popen("play -t raw -b 16 -c 1 -e s -r 48000 -", "w");

    // サーバーにダミーパケット送信して登録
    sendto(sockfd_client, "H", 1, 0,
           (struct sockaddr*)&srv, sizeof(srv));

    client_running = 1;
    pthread_t t_send, t_recv;
    pthread_create(&t_send, NULL, sender_loop, &srv);
    pthread_create(&t_recv, NULL, receiver_loop, NULL);
    pthread_join(t_send, NULL);
    pthread_join(t_recv, NULL);

    // 後始末
    pclose(rec_pipe);
    pclose(play_pipe);
    opus_encoder_destroy(opusEnc);
    opus_decoder_destroy(opusDec);
    rnnoise_destroy(dnSend);
    rnnoise_destroy(dnRecv);
    close(sockfd_client);
    return NULL;
}

// クライアント停止
void stop_client(void) {
    client_running = 0;
    if (sockfd_client >= 0) {
        close(sockfd_client);
        sockfd_client = -1;
    }
}
