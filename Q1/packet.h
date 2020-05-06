#ifndef __PACKET__
#define __PACKET__

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/select.h>

#define PACKET_SIZE 100
#define PORT 8888
// no of channels
#define MAX_CLIENTS 2
// timeout in seconds
#define TIMEOUT 2

// packet drop rate for channel 0 and channel 1 is 10% each
#define PDR0 10
#define PDR1 10
// packet buffer size: total size of packets that the buffer can accomodate
#define BUFFER_SIZE PACKET_SIZE*2

typedef struct {
    // payload, 1 byte extra to store '\0' which is not stored in output file
    char data[PACKET_SIZE + 1];
    int size;
    int seq_no;
    int is_last;
    // 0: data, 1: ACK
    int type;
    int channel_no;
} PACKET;

#endif
