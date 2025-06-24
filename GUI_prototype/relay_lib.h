#ifndef RELAY_LIB_H
#define RELAY_LIB_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Thread entry: arg is pointer to uint16_t (port number)
void *relay_thread(void *arg);

// Stop the relay loop
void stop_relay(void);

#endif
