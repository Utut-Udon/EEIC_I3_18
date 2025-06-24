#ifndef SERVER_LIB_H
#define SERVER_LIB_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>


// サーバー起動（バックグラウンドで pthread を作成して relay ループを回す）
void start_server(uint16_t port);

// サーバー停止（relay ループを止めてスレッドを join）
void stop_server(void);

#endif // SERVER_LIB_H
