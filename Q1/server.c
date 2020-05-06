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

// function to generate random number
double generate_random() {
    return (double)rand() / (double)((unsigned)RAND_MAX + 1);
}

int main() {

    struct sockaddr_in si_server, si_client;
    int server_socket;
    int client_socket[MAX_CLIENTS];

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
    // initializing all client sockets to 0
    for(i=0; i<MAX_CLIENTS; i++)
        client_socket[i] = 0;

    //  creating tcp socket
    // server socket or master socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server_socket == -1) {
        die("Socket Creation Error");
    }

    // setting server socket to allow multiple connections
    int opt = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {   
        die("setsockopt");
    }

    // local address structure
    memset((char*) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(PORT);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);

    // binding socket to port
    int bind_flag = bind(server_socket, (struct sockaddr*) &si_server, sizeof(si_server));
    if(bind_flag < 0) {
        die("Binding Error");
    }
    printf("Binding Successful\n");
    printf("IP: %s, Port: %d\n", inet_ntoa(si_server.sin_addr), (int)ntohs(si_server.sin_port));

    // listening for client
    int listen_flag = listen(server_socket, MAX_CLIENTS);
    if(listen_flag < 0) {
        die("Listening Error");
    }

    // variable to keep track of when to close server
    int quit = 0;
    // next expected in order sequence number
    // first sequence number is 0
    int next_seq = 0;
    
    while(1) {

        // master file descriptor
        fd_set master;
        FD_ZERO(&master);
        FD_SET(server_socket, &master);
        int max_sd = server_socket, sd, flag = 1;

        for(i=0; i<MAX_CLIENTS; i++) {
            sd = client_socket[i];
            
            // add if valid socket descriptor
            if(sd > 0) 
                FD_SET(sd, &master);

            if(sd > 0 && quit) {
                flag = 0;
            }

            // update max value of socket descriptor
            // required for select
            if(sd > max_sd)
                max_sd = sd;
        }

        if(flag && quit)
            break;
        
        int socket_count = select(max_sd + 1, &master, NULL, NULL, NULL);
        
        if(socket_count < 0) {
            die("Select Error");
        }

        // if server_socket is set, then it is an incoming connection
        if(FD_ISSET(server_socket, &master)) {
            int client_length = sizeof(si_client);
            int client_sock = accept(server_socket, (struct sockaddr*) &si_client, &client_length);
            if(client_length < 0) {
                die("Client Socket Error");
            }

            for(i=0; i<MAX_CLIENTS; i++) {
                if(client_socket[i] == 0) {
                    client_socket[i] = client_sock;
                    break;
                }
            }

            printf("Handling Client at %d with IP: %s, Port: %d\n", client_sock, inet_ntoa(si_client.sin_addr), (int)ntohs(si_client.sin_port));
        }

        // if server_socket is not set, then select is set for some IO operation on another socket
        for(i=0; i<MAX_CLIENTS; i++) {
            sd = client_socket[i];
            
            // check if socket is set
            // if it is, then receive the message
            if(sd && FD_ISSET(sd, &master)) {
                PACKET send_pkt;
                PACKET* rcv_pkt = (PACKET*) malloc(sizeof(PACKET));
                int sent, received;

                // receiving data, blocking call
                received = recv(sd, rcv_pkt, sizeof(*rcv_pkt), 0);
                if(received < 0) {
                    die("Receive Error");
                }
                else if(received == 0 && quit) {
                    close(sd);
                    client_socket[i] = 0;
                    FD_CLR(sd, &master);
                    continue;
                }

                // random number generated to drop packets
                double r = generate_random();
                double prob1 = (double) PDR0 / 100, prob2 = (double) PDR1/100;

                if((r <= prob1 && rcv_pkt->channel_no == 0) || (r <= prob2 && rcv_pkt->channel_no == 1)) {
                    // drop packet i.e. do not send acknowledge
                    printf("DROP PKT: from channel %d\n", rcv_pkt->channel_no);
                    continue;
                }

                // for debugging, prints content of received packet
                // int index = 0;
                // for(index=0; index<rcv_pkt->size; index++) {
                //     printf("%c", rcv_pkt->data[index]);
                // }
                // printf("\n");

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
                else {
                    // if buffer is full, then drop packet
                    if(head.count == head.size) {
                        printf("BUFFER FULL, DROP PKT: from channel %d\n", rcv_pkt->channel_no);
                        continue;
                    }
                    else {
                        // if it is not the expected packet, add it to the buffer
                        insert_in_order(&head, rcv_pkt);
                    }
                }
                // printf("\nNext sequence number: %d\n", next_seq);
                // printf("Packet received from channel number %d from client %d, sequence %d\n", rcv_pkt->channel_no, sd, rcv_pkt->seq_no);
                printf("RCVD PKT: Seq No. %-4d of size %-4d Bytes from channel %d\n", rcv_pkt->seq_no, rcv_pkt->size, rcv_pkt->channel_no);

                send_pkt.seq_no = rcv_pkt->seq_no;
                send_pkt.channel_no = rcv_pkt->channel_no;
                send_pkt.type = 1;
                
                sent = send(sd, &send_pkt, sizeof(send_pkt), 0);
                if(sent != sizeof(send_pkt)) {
                    die("Sending Error");
                }
                // printf("ACK Sent\n");
                printf("SENT ACK: for PKT with Seq No. %-4d from channel %d\n", rcv_pkt->seq_no, rcv_pkt->channel_no);

                if(rcv_pkt->is_last == 1 || quit == 1) {
                    close(sd);
                    quit = 1;
                    client_socket[i] = 0;
                    FD_CLR(sd, &master);
                    continue;
                }
            }
        }
    }

    // close the client channels
    printf("\nClosing Server\n");
    for(i=0; i<MAX_CLIENTS; i++) {
        if(client_socket[i] != 0) {
            close(client_socket[i]);
        }
    }
    // close the server socket
    fclose(fp);
    close(server_socket);

    return 0;
}