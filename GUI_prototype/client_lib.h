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

// Thread entry: arg is pointer to a "ip:port" string
void *client_thread(void *arg);

// Stop client loops
void stop_client(void);

#endif
