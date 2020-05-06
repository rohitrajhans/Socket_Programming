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
// server's port number
#define PORT 8880
// as relay nodes are also running on the same machine, they cannot have different IPs
// relay node 1s port number
#define PORT_RELAY_1 8881
// relay node 2s port number
#define PORT_RELAY_2 8882
// no of relay channels
#define MAX_CLIENTS 2
// packet buffer size: total size of packets that the buffer can accomodate
#define BUFFER_SIZE PACKET_SIZE*2

// packet drop rate for relay node 0 and relay node 1 is 10% each
#define PDR0 10
#define PDR1 10

// relay nodes add a delay b/w 0 and DELAY (2) ms
#define DELAY 2

// timeout in seconds
#define TIMEOUT 2

// windows size for SR protocol
// as it is not specified in question, it is set to be server buffer size - 1
// since server can buffer at most BUFFER_SIZE out-of-order packets at once
#define WINDOW_SIZE (BUFFER_SIZE / PACKET_SIZE) + 1

typedef struct {
    // payload, 1 byte extra to store '\0' which is not stored in output file
    char data[PACKET_SIZE + 1];
    int size;
    // next byte
    int seq_no;
    // sequence number of packet in selective repeat protocol
    int sr_seq;
    // packet number
    int pkt_no;
    int is_last;
    // 0: data, 1: ACK
    int type;
    int channel_no;
} PACKET;

// function to record log
int record_log(char* node_name, char* event_type, char* timestamp, char* pkt_type, int pkt_no, int seq_no, char* source, char* dest);
// to get current time
char* get_current_time();

#endif
