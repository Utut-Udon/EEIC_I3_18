#ifndef CLIENT_LIB_H
#define CLIENT_LIB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include </opt/homebrew/opt/opus/include/opus/opus.h>
#include <rnnoise.h>

// arg は "ip:port" 形式の文字列へのポインタ
void *client_thread(void *arg);

// 送受信ループを止める
void stop_client(void);

#endif // CLIENT_LIB_H

