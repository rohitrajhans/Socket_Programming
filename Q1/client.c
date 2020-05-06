#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/select.h>
#include<sys/time.h>
#include<time.h>

#include "packet.h"

typedef struct {
    PACKET pkt_data;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval timeout;
} packet_list;

void die(char *msg) {
    perror(msg);
    exit(1);
}

int get_minimum_timeout(packet_list* list, int* client_socket) {
    int i, j=0;
    struct timeval temp;
    temp.tv_sec = 100;
    temp.tv_usec = 0;

    for(i=0; i<MAX_CLIENTS; i++) {
        if( client_socket[i] && timercmp(&list[i].timeout, &temp, <)) {
            if(list[i].timeout.tv_sec < 0) {
                // adding some buffer time for in case of multiple drops
                list[i].timeout.tv_sec = 0;
                list[i].timeout.tv_usec = 500;    
            }
            temp = list[i].timeout;
            j = i;
        }
        // printf("%ld.%ld\n", list[i].timeout.tv_sec, list[i].timeout.tv_usec);
    }

    return j;
}


int flush_string(char* str, int size) {
    int i;
    for(i=0; i<size; i++) {
        str[i] = 0;
    }

    return 1;
}

int load_data(FILE* fp, char* str, int size) {

    // flush string before reading
    flush_string(str, size);
    // read size bytes into string
    int read = fread(str, sizeof(char), size, fp);

    // if(read < (size * sizeof(char))) {
    //     // if end of file is encountered return 0
    //     return 0;
    // }

    // return number of bytes read
    return read;
}

int main() {

    struct sockaddr_in si_server;
    int client_socket[MAX_CLIENTS], i;
    char buf[PACKET_SIZE];

    // file to be transferred to server
    FILE* fp = fopen("input.txt", "r");
    if(fp == NULL) {
        die("File Opening Error");
    }

    PACKET rcv_pkt;

    int j = 0, k = 0;
    // char* message[message_no] = {"msg1", "msg2", "msg3",
    // "msg4", "msg5", "msg6", "msg7", "msg8", "msg9", "msg10"};

    // creating client tcp sockets
    // 1 socket for each channel
    for(i=0; i<MAX_CLIENTS; i++) {
        if((client_socket[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            die("Socket Opening Error");
        }
    }

    // server address architecture
    memset((char*) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(PORT);
    si_server.sin_addr.s_addr = inet_addr("127.0.0.1");

    // establishing connection
    for(i=0; i<MAX_CLIENTS; i++) {
        int c  = connect(client_socket[i], (struct sockaddr*) &si_server, sizeof(si_server));
        if(c < 0) {
            die("Connection Error");
        }    
    }
    printf("Connection Established\n");

    int sent, received, quit = 0;
    // packet list holds data about packets currently in the channel
    // each channel can have at most one packet
    packet_list list[MAX_CLIENTS];

    // master file descriptor
    fd_set master;
    FD_ZERO(&master);
    int max_sd = 0;
    
    // add client socket descriptor
    // and send initial messages
    // assuming there are atleast 2 packets
    for(i=0; i<MAX_CLIENTS; i++) {
        PACKET send_pkt;

        FD_SET(client_socket[i], &master);
        max_sd = max_sd > client_socket[i] ? max_sd : client_socket[i];
        
        int bytes_read = load_data(fp, send_pkt.data, PACKET_SIZE);
        send_pkt.channel_no = i;
        // sequence number is offset of 1st byte
        send_pkt.seq_no = k;
        k = k + bytes_read;

        send_pkt.size = bytes_read;
        send_pkt.type = 0;

        if(bytes_read == 0) {
            send_pkt.is_last = 1;
            quit = 1;
        }
        else
            send_pkt.is_last = 0;

        // initialize the time variables
        list[i].pkt_data = send_pkt;
        gettimeofday(&list[i].start_time, 0);
        list[i].timeout.tv_sec = TIMEOUT;
        list[i].timeout.tv_usec = 0;
        timeradd(&list[i].timeout, &list[i].start_time, &list[i].end_time);

        sent = send(client_socket[i], &send_pkt, sizeof(send_pkt), 0);
        if(sent != sizeof(send_pkt)) {
            die("Send Error");
        }
        printf("SENT PKT: Seq No. %-4d of size %-3d Bytes from channel %d\n", send_pkt.seq_no, send_pkt.size, send_pkt.channel_no);
    }

    while(1) {
        
        // if master file descriptor is empty then, all ACKs have been received
        fd_set empty;
        FD_ZERO(&empty);
        if(memcmp(&master, &empty, sizeof(fd_set)) == 0 && quit) {
            break;
        }

        fd_set copy = master;

        int min_index = get_minimum_timeout(list, client_socket);
        // timeout is set to minimum value of timeout for all packets currently in channel
        struct timeval tv = list[min_index].timeout;
        // printf("%ld.%ld\n", list[min_index].timeout.tv_sec, list[min_index].timeout.tv_usec);
        int socket_count = select(max_sd + 1, &copy, NULL, NULL, &tv);
        // printf("Socket Count: %d\n", socket_count);

        if(socket_count < 0) {
            die("Select Error");
        }

        // handling timeout
        if(socket_count == 0) {
            printf("TIMEOUT : Seq No. %-4d of size %-3d Bytes from channel %d\n", list[min_index].pkt_data.seq_no, list[min_index].pkt_data.size, list[min_index].pkt_data.channel_no);
            
            struct timeval curr_time, time_spent;
            gettimeofday(&curr_time, 0);
            for(i=0; i<MAX_CLIENTS; i++) {
                // reduce timeout value for all packets in channel
                timersub(&curr_time, &list[i].start_time, &time_spent);
                timersub(&list[i].timeout, &time_spent, &list[i].timeout);
            }

            // reset timer
            list[min_index].start_time = curr_time;
            list[min_index].timeout.tv_sec = TIMEOUT;
            list[min_index].timeout.tv_usec = 0;
            timeradd(&list[min_index].start_time, &list[min_index].timeout, &list[min_index].end_time);
            // printf("Hello %ld.%ld\n", list[min_index].timeout.tv_sec, list[min_index].timeout.tv_usec);

            FD_CLR(client_socket[min_index], &master);

            // resend packet
            sent = send(client_socket[min_index], &list[min_index].pkt_data, sizeof(list[min_index].pkt_data), 0);
            if(sent != sizeof(list[min_index].pkt_data)) {
                die("Send Error");
            }
            // printf("Data sent (resend): %d\n", list[min_index].pkt_data.channel_no);
            printf("RESENT  : Seq No. %-4d of size %-3d Bytes from channel %d\n", list[min_index].pkt_data.seq_no, list[min_index].pkt_data.size, list[min_index].pkt_data.channel_no);
            FD_SET(client_socket[min_index], &master);
        }

        // select is set for some IO operation on client channels
        if(socket_count > 0) {
            for(i=0; i<MAX_CLIENTS; i++) {
                PACKET send_pkt;

                int sock = client_socket[i];

                // reduce timeout value
                struct timeval curr_time, time_spent;
                gettimeofday(&curr_time, 0);
                timersub(&curr_time, &list[i].start_time, &time_spent);
                timersub(&list[i].timeout, &time_spent, &list[i].timeout);

                // check if client channel is set
                if(sock && FD_ISSET(sock, &copy)) {
                    received = recv(sock, &rcv_pkt, sizeof(rcv_pkt), 0);
                    
                    if(received < 0) {
                        die("Receive Error");
                    }
                    // printf("Expected ACK received: %d\n", rcv_pkt.channel_no);
                    printf("RCVD ACK: for PKT with Seq No. %-4d from channel %d\n", rcv_pkt.seq_no, rcv_pkt.channel_no);
                    FD_CLR(sock, &master);

                    if(quit) {
                        client_socket[i] = 0;
                        continue;
                    }

                    // loading packet
                    int bytes_read = load_data(fp, send_pkt.data, PACKET_SIZE);
                    send_pkt.channel_no = i;
                    // sequence number is offset of 1st byte
                    send_pkt.seq_no = k;
                    k = k + bytes_read;

                    send_pkt.size = bytes_read;
                    send_pkt.type = 0;

                    if(bytes_read == 0) {
                        send_pkt.is_last = 1;
                        quit = 1;
                    }
                    else
                        send_pkt.is_last = 0;

                    // initialize the time variables
                    list[i].pkt_data = send_pkt;
                    gettimeofday(&list[i].start_time, 0);
                    list[i].timeout.tv_sec = TIMEOUT;
                    list[i].timeout.tv_usec = 0;
                    timeradd(&list[i].start_time, &list[i].timeout, &list[i].end_time);

                    sent = send(sock, &send_pkt, sizeof(send_pkt), 0);
                    if(sent != sizeof(send_pkt)) {
                        die("Send Error");
                    }
                    // printf("Data sent: %d\n", send_pkt.channel_no);
                    printf("SENT PKT: Seq No. %-4d of size %-3d Bytes from channel %d\n", send_pkt.seq_no, send_pkt.size, send_pkt.channel_no);

                    FD_SET(sock, &master);
                }
            }
        }
    }

    printf("\nClosing Client\n");
    fclose(fp);
    for(i=0; i<MAX_CLIENTS; i++)
        close(client_socket[i]);
    
    return 0;
}