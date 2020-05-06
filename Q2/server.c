#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<sys/select.h>
#include<time.h>

#include "packet.h"

struct packet_buffer {
    PACKET* pkt;
    struct packet_buffer* next;
};

typedef struct packet_buffer* Buffer;

typedef struct {
    Buffer top;
    // Max Buffer Size
    int count;
    int size;
} BUFFER_HEAD;

// maintains order wrt sequence number in buffer
int insert_in_order(BUFFER_HEAD *head, PACKET* pkt) {
    Buffer new_buf = (Buffer) malloc(sizeof(struct packet_buffer));

    if(head->top == NULL) {
        new_buf->pkt = pkt;
        new_buf->next = NULL;
        head->top = new_buf;
        head->count += 1;
        
        return 1;
    }

    Buffer temp = head->top;

    while(temp->next && (temp->next->pkt->seq_no < pkt->seq_no)) {
        temp = temp->next;
    }

    if(head->top->pkt->seq_no > pkt->seq_no) {
        new_buf->pkt = pkt;
        new_buf->next = head->top;
        head->top = new_buf;
    }
    else {
        new_buf->pkt = pkt;
        new_buf->next = temp->next;
        temp->next = new_buf;
    }

    head->count += 1;

    return 1;
}

int write_to_file(FILE* fp, BUFFER_HEAD *head, int *seq_no) {
    Buffer temp = head->top;
    int bytes_written = 0;

    // for debugging
    // while(temp) {
    //     printf("%d\t", temp->pkt->seq_no);
    //     temp = temp->next;
    // }
    // printf("\n");
    // temp = head->top;

    while(temp) {
        
        // printf("write: %d %d\n", temp->pkt->seq_no, *seq_no);

        // write only if inorder
        if(temp->pkt->seq_no != (*seq_no)) {
            break;
        }

        bytes_written += fwrite(temp->pkt->data, sizeof(char), temp->pkt->size, fp);
        *seq_no += temp->pkt->size;
        
        Buffer temp2 = temp;
        temp = temp->next;
        head->top = temp;
        head->count -= 1;
        free(temp2);
    }

    return 1;
}

void die(char *msg) {
    perror(msg);
    exit(1);
}

double generate_random() {
    return (double)rand() / (double)((unsigned)RAND_MAX + 1);
}

int main() {

    struct sockaddr_in si_server;
    int server_socket;

    // file for writing
    FILE* fp = fopen("output.txt", "w");
    if(fp == NULL) {
        die("File Opening Error");
    }

    // head of buffer required for writing
    // only out of order packets are stored in buffer
    // in order packets are directly written to file
    BUFFER_HEAD head;
    head.top = NULL;
    head.size = (int)(BUFFER_SIZE / PACKET_SIZE);
    head.count = 0;
    
    // using current time as seed for random generator
    // random generator used later
    srand(time(0));

    int i;

    // creating server socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1) {
        die("Socket Creation Error");
    }

    // initialize server address structure
    memset((char*) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(PORT);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    int bind_bool = bind(sock, (struct sockaddr*) &si_server, sizeof(si_server));
    if(bind_bool == -1)
        die("Binding Error");
    printf("Binding Successful\n");
    printf("IP: %s, Port: %d\n", inet_ntoa(si_server.sin_addr), (int)ntohs(si_server.sin_port));

    int quit = 0, next_seq = 0;
    // log information
    char* src[] = {"RELAY1", "RELAY2"};

    while(1) {

        // if last packet has been received and buffer is empty (no other packet left) then quit
        if(quit && head.count == 0) {
            break;
        }

        PACKET send_pkt;
        PACKET* rcv_pkt = (PACKET*) malloc(sizeof(PACKET));
        struct sockaddr_in si_rel;
        int slen = sizeof(si_rel);

        int received = recvfrom(sock, rcv_pkt, sizeof(*rcv_pkt), 0, (struct sockaddr*) &si_rel, &slen);
        if(received == -1)
            die("Receive Error");

        // for debugging
        // printf("Next Expected Sequence: %d\n", next_seq);
        if(rcv_pkt->seq_no == next_seq) {
            // if buffer is empty, directly write to file
            if(head.top == NULL) {
                int bytes_written = fwrite(rcv_pkt->data, sizeof(char), rcv_pkt->size, fp);
                next_seq += rcv_pkt->size;
            }
            else {
                insert_in_order(&head, rcv_pkt);
                write_to_file(fp, &head, &next_seq);
            }
        }
        else if(rcv_pkt->seq_no > next_seq) {
            // if buffer is full, then drop out-of-order packet
            if(head.count == head.size) {
                printf("DROP PKT: Seq No. %-4d from %-6s to %-6s of size %-3d\n", rcv_pkt->seq_no, src[rcv_pkt->channel_no], "SERVER", rcv_pkt->size);
                record_log("SERVER", "D", get_current_time(), "DATA", rcv_pkt->pkt_no, rcv_pkt->seq_no, src[rcv_pkt->channel_no], "SERVER");
                continue;
            }
            else {
                // if it is not expected packet, add it to the buffer
                insert_in_order(&head, rcv_pkt);
            }
        }
        else if(rcv_pkt->seq_no < next_seq) {
            // do nothing
            // just send back ack
        }
        // printf("Packet received from relay node %d, sequence %d\n", rcv_pkt->channel_no, rcv_pkt->seq_no);
        printf("RCVD PKT: Seq No. %-4d from %-6s to %-6s of size %-3d\n", rcv_pkt->seq_no, src[rcv_pkt->channel_no], "SERVER", rcv_pkt->size);
        record_log("SERVER", "R", get_current_time(), "DATA", rcv_pkt->pkt_no, rcv_pkt->seq_no, src[rcv_pkt->channel_no], "SERVER");

        // Prepare ACK to be sent
        send_pkt.seq_no = rcv_pkt->seq_no;
        send_pkt.sr_seq = rcv_pkt->sr_seq;
        send_pkt.type = 1;
        send_pkt.pkt_no = rcv_pkt->pkt_no;
        send_pkt.channel_no = rcv_pkt->channel_no;
        
        // Send ACK on channel which received the packet
        int sent = sendto(sock, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr*) &si_rel, slen);
        if(sent == -1)
            die("Sending Error");
        // printf("ACK Sent\n");
        printf("SENT ACK: Seq No. %-4d from %-6s to %-6s\n", send_pkt.seq_no, "SERVER", src[send_pkt.channel_no]);
        record_log("SERVER", "S", get_current_time(), "ACK", send_pkt.pkt_no, send_pkt.seq_no, "SERVER", src[send_pkt.channel_no]);

        if(rcv_pkt->is_last == 1) {
            quit = 1;
        }

        // for debugging
        // printf("isLast: %d, Quit: %d, Head: %d\n", rcv_pkt->is_last, quit, head.count);
    }

    printf("Closing Server\n");
    fclose(fp);
    close(sock);
    return 0;
}