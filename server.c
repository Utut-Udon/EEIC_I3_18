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
#include <stdbool.h>

#define N 256
#define MAX_CLIENTS 10

typedef struct
{
    int sock;
    FILE *pipe;
} thread_arg_t;

typedef struct
{
    int sock;
    pthread_t send_tid;
    pthread_t recv_tid;
    bool connected;
    unsigned char current_recv_buffer[N];
    pthread_mutex_t recv_buffer_mutex;
    pthread_cond_t recv_buffer_cond;
    bool recv_buffer_full;
    long last_sent_frame_id; // このクライアントが最後に送信したフレームID
} client_info_t;

client_info_t clients[MAX_CLIENTS];
int num_connected_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned char global_mixed_audio[N];
pthread_mutex_t mix_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t mix_buffer_cond = PTHREAD_COND_INITIALIZER;
volatile bool new_mix_available = false;
long current_frame_id = 0; // 現在のミックスフレームのID

void *client_recv_thread(void *arg)
{
    client_info_t *client = (client_info_t *)arg;
    unsigned char data[N];
    ssize_t n;
    // キューの代わりに、ここでは簡易的に単一バッファを使用
    pthread_mutex_init(&client->recv_buffer_mutex, NULL);
    pthread_cond_init(&client->recv_buffer_cond, NULL);
    client->recv_buffer_full = false;

    while (client->connected && (n = read(client->sock, data, sizeof(data))) > 0)
    {
        // ここで受信した `data` をミキシングキュー（または直接グローバルバッファ）に送る
        pthread_mutex_lock(&client->recv_buffer_mutex);
        // バッファがまだ消費されていない場合は待機
        while (client->recv_buffer_full)
        {
            pthread_cond_wait(&client->recv_buffer_cond, &client->recv_buffer_mutex);
        }

        memcpy(client->current_recv_buffer, data, n); // 受信データを自身のバッファにコピー
        client->recv_buffer_full = true;
        pthread_cond_signal(&client->recv_buffer_cond); // ミキシングスレッドにデータが利用可能になったことを通知
        pthread_mutex_unlock(&client->recv_buffer_mutex);
    }

    // クライアント切断時のクリーンアップ
    perror("client disconnected or read error");
    pthread_mutex_lock(&clients_mutex);
    client->connected = false;
    close(client->sock);
    num_connected_clients--;
    pthread_mutex_unlock(&clients_mutex);
    pthread_exit(NULL);
}

void *client_send_thread(void *arg)
{
    client_info_t *client = (client_info_t *)arg;
    unsigned char data_to_send[N];
    ssize_t sent = 0;
    // クライアントが接続された直後、現在のフレームIDを同期させる

    pthread_mutex_lock(&mix_buffer_mutex);
    client->last_sent_frame_id = current_frame_id; // 初期値として現在のフレームIDを設定
    pthread_mutex_unlock(&mix_buffer_mutex);

    while (client->connected)
    {
        pthread_mutex_lock(&mix_buffer_mutex);
        // 新しいミックスデータが来るまで待機
        while (client->last_sent_frame_id >= current_frame_id && client->connected)
        {
            pthread_cond_wait(&mix_buffer_cond, &mix_buffer_mutex);
        }

        if (!client->connected)
        { // 待機中に切断された場合
            pthread_mutex_unlock(&mix_buffer_mutex);
            break;
        }

        // ミックスされたデータをコピーして送信（スレッドセーフにするため）
        memcpy(data_to_send, global_mixed_audio, N);
        client->last_sent_frame_id = current_frame_id; // 送信するフレームIDを更新
        pthread_mutex_unlock(&mix_buffer_mutex);

        sent = 0;
        while (sent < N)
        {
            ssize_t m = write(client->sock, data_to_send + sent, N - sent);
            if (m < 0)
            {
                perror("client_send_thread write error");
                pthread_mutex_lock(&clients_mutex);
                client->connected = false;
                close(client->sock);
                num_connected_clients--;
                pthread_mutex_unlock(&clients_mutex);
                pthread_exit(NULL);
            }
            sent += m;
        }
    }
    pthread_exit(NULL);
}

// ミキシングスレッド
void *mixer_thread(void *arg)
{
    // global_mixed_audio はこのスレッドが管理
    unsigned char temp_mixed_audio[N]; // 一時的なミキシングバッファ
    while (true)
    {
        memset(global_mixed_audio, 0, N); // まずはゼロクリア
        short temp_samples[N / 2]; // 一時的にshort型のサンプルを保持する配列
        memset(temp_samples, 0, sizeof(temp_samples)); // ゼロクリア
        pthread_mutex_lock(&mix_buffer_mutex); // global_mixed_audio と new_mix_available を保護
        new_mix_available = false; // ★ここを追加または変更★
        pthread_mutex_unlock(&mix_buffer_mutex);
        int active_clients_count = 0; // 現在、音声データを送ってきているクライアントの数
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].connected)
            {
                pthread_mutex_lock(&clients[i].recv_buffer_mutex);
                if (clients[i].recv_buffer_full)
                {
                    active_clients_count++; // このクライアントは今回音声データを送ってきた
                    for (int j = 0; j < N / 2; j++)
                    {
                        short incoming_sample = (short)(clients[i].current_recv_buffer[j * 2] | (clients[i].current_recv_buffer[j * 2 + 1] << 8));
                        temp_samples[j] += incoming_sample; // まずはshort型で加算
                    }
                    clients[i].recv_buffer_full = false;
                    pthread_cond_signal(&clients[i].recv_buffer_cond);
                }
                pthread_mutex_unlock(&clients[i].recv_buffer_mutex);
            }
        }

        pthread_mutex_unlock(&clients_mutex);
        // 加算後のサンプルを正規化し、global_mixed_audioに格納
        for (int j = 0; j < N / 2; j++)
        {
            long mixed_result_long = (long)temp_samples[j]; // 加算結果
            // 正規化: active_clients_countが0の場合はスキップ、そうでない場合は割る
            if (active_clients_count > 0)
            {
                mixed_result_long /= active_clients_count;
            }

            // (active_clients_count <= 0 の場合は temp_samples[j] は0なので、mixed_result_longも0)
            // クリッピング (正規化後にも念のため)
            if (mixed_result_long > 32767)
                mixed_result_long = 32767;
            else if (mixed_result_long < -32768)
                mixed_result_long = -32768;
            // byte配列に戻す

            global_mixed_audio[j * 2] = (unsigned char)(mixed_result_long & 0xFF);
            global_mixed_audio[j * 2 + 1] = (unsigned char)((mixed_result_long >> 8) & 0xFF);
        }

        // 3. ミックスされたデータが利用可能になったことを送信スレッドに通知
        pthread_mutex_lock(&mix_buffer_mutex);
        current_frame_id++; // 新しいフレームができた！
        pthread_cond_broadcast(&mix_buffer_cond); // 全ての送信スレッドに通知
        pthread_mutex_unlock(&mix_buffer_mutex);

        // ミキシング周期の調整（例: 10msごとにミックス）
        usleep(2500); // Nが256バイト(128サンプル) 44.1kHzの場合、約2.9msに1回処理すべき
        // Nとサンプリングレートから適切な処理間隔を計算し、それより短い間隔でポーリングまたは待機
        // 音声のリアルタイム性を考えると、各クライアントからデータを受信するたびにミックスをトリガーするのが理想的
        // または、タイマーと条件変数で定期的にミックス処理を実行する
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <Port>\n", argv[0]);
        exit(1);
    }

    int s;
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

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].connected = false;
        pthread_mutex_init(&clients[i].recv_buffer_mutex, NULL);
        pthread_cond_init(&clients[i].recv_buffer_cond, NULL);
        clients[i].recv_buffer_full = false;
    }

    pthread_t mixer_tid;
    // mixer_thread には引数を渡さない、または必要に応じてNULL
    if (pthread_create(&mixer_tid, NULL, mixer_thread, NULL) != 0)
    {
        perror("pthread_create mixer_thread");
        exit(1);
    }

    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int s = accept(ss, (struct sockaddr *)&client_addr, &len); // 新しいクライアントソケット
        if (s == -1)
        {
            perror("accept");
            continue; // エラーが発生してもループを続ける
        }

        pthread_mutex_lock(&clients_mutex); // クライアントリストへのアクセスを保護

        // 空いているクライアントスロットを探す
        int client_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].connected)
            {
                client_idx = i;
                break;
            }
        }

        if (client_idx != -1)
        {
            // スロットが見つかった場合、情報を設定してスレッドを起動
            clients[client_idx].sock = s;
            clients[client_idx].connected = true;
            num_connected_clients++;

            // 各クライアント専用の送受信スレッドを起動
            pthread_create(&clients[client_idx].recv_tid, NULL, client_recv_thread, &clients[client_idx]);
            pthread_create(&clients[client_idx].send_tid, NULL, client_send_thread, &clients[client_idx]);
        }
        else
        {
            // 最大クライアント数に達した場合
            fprintf(stderr, "Max clients reached. Rejecting new connection.\n");
            close(s); // 接続を拒否
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // ここは通常到達しない (無限ループのため)
    close(ss);
    return 0;
}
