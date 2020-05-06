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

int get_minimum_timeout(packet_list* list, int* is_valid, int ws) {
    int i, j=0;
    struct timeval temp;
    temp.tv_sec = 100;
    temp.tv_usec = 0;

    for(i=0; i<ws; i++) {
        if( is_valid[i] && timercmp(&list[i].timeout, &temp, <)) {
            if(list[i].timeout.tv_sec < 0) {
                // adding some buffer time for in case of multiple drops
                // also maintaining the original order
                list[i].timeout.tv_sec = 0;
                list[i].timeout.tv_usec = 500 - i;    
            }
            temp = list[i].timeout;
            j = i;
        }
        // printf("%d %d: %ld.%ld\n", list[i].pkt_data.seq_no, is_valid[i], list[i].timeout.tv_sec, list[i].timeout.tv_usec);
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

    // file to be transferred to server
    FILE* fp = fopen("input.txt", "r");
    if(fp == NULL) {
        die("File Opening Error");
    }

    // log file to store traces
    FILE* fp_log = fopen("log.txt", "w");
    if(fp_log == NULL) {
        printf("Unable to create log.txt, continuing without it\n");
    }
    fclose(fp_log);
    // store log titles
    record_log("Node Name", "Event Type", "Timestamp", "Packet Type", -1, -1, "Source", "Dest");

    struct sockaddr_in si_relay[MAX_CLIENTS];
    int i;
    PACKET rcv_pkt;

    int port_nos[MAX_CLIENTS];
    // currently setup for relay nodes
    // if more relay nodes are present i.e. MAX_CLIENTS is changed then update the values as well
    port_nos[0] = PORT_RELAY_1;
    port_nos[1] = PORT_RELAY_2;

    // create socket at client side
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1) {
        die("Socket Creation");
    }

    // relay nodes address architecture
    // MAX_CLIENTS no of relay nodes
    for(i=0; i<MAX_CLIENTS; i++) {
        memset((char*) &si_relay[i], 0, sizeof(si_relay[i]));
        si_relay[i].sin_family = AF_INET;
        si_relay[i].sin_port = htons(port_nos[i]);
        // relay nodes at local address
        // if ip of relay nodes is changed, update here
        si_relay[i].sin_addr.s_addr = inet_addr("127.0.0.1");

        // printf("IP: %s, Port: %d\n", inet_ntoa(si_relay[i].sin_addr), (int)ntohs(si_relay[i].sin_port));
    }
    
    // window size for selective repeat protocol
    int ws = (int) WINDOW_SIZE;

    // assuming 2 relay nodes
    // node 0 - for odd packets
    // node 1 - for even packets

    // slot information
    int available_slot = 0, expected_ack = 0, quit = 0;
    // keeps track of number of packets sent
    int j = 0;
    // keeps track of number of bytes sent
    int k = 0;
    // list of active packets i.e. packets sent but not acked
    packet_list list[ws];
    // information to store log
    char* dest[] = {"RELAY1", "RELAY2"};
    char* pkt_type[] = {"DATA", "ACK"};

    int acked[ws], is_valid[ws];
    memset(acked, 0, sizeof(int) * ws);
    memset(is_valid, 0, sizeof(int) * ws);

    for(i=0; i<ws; i++) {
        PACKET send_pkt;

        if(quit)
            break;

        int bytes_read = load_data(fp, send_pkt.data, PACKET_SIZE);

        if(bytes_read == 0) {
            send_pkt.is_last = 1;
            quit = 1;
        }
        else
            send_pkt.is_last = 0;
        send_pkt.channel_no = 1 - (j % 2);
        send_pkt.seq_no = k;
        send_pkt.sr_seq = i;
        send_pkt.type = 0;
        send_pkt.size = bytes_read;
        send_pkt.pkt_no = j+1;

        k += bytes_read;
        j += 1;

        is_valid[i] = 1;

        // initialize the time variables
        list[i].pkt_data = send_pkt;
        gettimeofday(&list[i].start_time, 0);
        list[i].timeout.tv_sec = TIMEOUT;
        list[i].timeout.tv_usec = 0;
        timeradd(&list[i].timeout, &list[i].start_time, &list[i].end_time);

        int sent = sendto(sock, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *) &(si_relay[send_pkt.channel_no]), sizeof(si_relay[send_pkt.channel_no]));
        if(sent != sizeof(send_pkt))
            die("Packet Sending Error");

        // printf("Data Sent: %d\n", send_pkt.channel_no);
        printf("SENT PKT: Seq No. %-4d from %-6s to %-6s of size %-3d\n", send_pkt.seq_no, "CLIENT", dest[send_pkt.channel_no], send_pkt.size);
        record_log("CLIENT", "S", get_current_time(), "DATA", send_pkt.pkt_no, send_pkt.seq_no, "CLIENT", dest[send_pkt.channel_no]);

        available_slot += 1;

        if(available_slot == ws)
            available_slot = 0;
    }

    if(quit) {
        close(sock);
        return 1;
    }

    while(1) {
        
        // if all packets have been sent successfully, break from loop
        int valid = 0;
        for(i=0; i<ws; i++) {
            if(is_valid[i] == 1) {
                valid = 1;
                break;
            }
        }

        if(!valid && quit) {
            break;
        }

        fd_set master;
        FD_ZERO(&master);
        FD_SET(sock, &master);
        int min_index = get_minimum_timeout(list, is_valid, ws);
        // printf("Timeout %d: %ld.%ld\n", list[min_index].pkt_data.seq_no, list[min_index].timeout.tv_sec, list[min_index].timeout.tv_usec);
        struct timeval tv = list[min_index].timeout;
        int socket_count = select(sock + 1, &master, NULL, NULL, &tv);

        if(socket_count < 0) {
            die("Select Error");
        }
        
        else if(socket_count == 0) {
            // handling timeout

            printf("TIMEOUT : Seq No. %-4d from %-6s to %-6s of size %-3d\n", list[min_index].pkt_data.seq_no, "CLIENT", dest[list[min_index].pkt_data.channel_no], list[min_index].pkt_data.size);
            record_log("CLIENT", "TO", get_current_time(), "DATA", list[min_index].pkt_data.pkt_no, list[min_index].pkt_data.seq_no, "CLIENT", dest[list[min_index].pkt_data.channel_no]);

            struct timeval curr_time, time_spent;
            gettimeofday(&curr_time, 0);
            for(i=0; i<ws; i++) {
                if(is_valid[i]) {
                    // reduce timeout value for all packets in channel
                    timersub(&curr_time, &list[i].start_time, &time_spent);
                    timersub(&list[i].timeout, &time_spent, &list[i].timeout);
                }
            }

            // reset timer
            list[min_index].start_time = curr_time;
            list[min_index].timeout.tv_sec = TIMEOUT;
            list[min_index].timeout.tv_usec = 0;
            timeradd(&list[min_index].start_time, &list[min_index].timeout, &list[min_index].end_time);
            // printf("Hello %d: %ld.%ld\n", list[min_index].pkt_data.seq_no, list[min_index].timeout.tv_sec, list[min_index].timeout.tv_usec);

            int sent = sendto(sock, &(list[min_index].pkt_data), sizeof(list[min_index].pkt_data), 0, 
            (struct sockaddr *) &(si_relay[list[min_index].pkt_data.channel_no]), sizeof(si_relay[list[min_index].pkt_data.channel_no]));
            if(sent != sizeof(list[min_index].pkt_data))
                die("Sending Error");
            // printf("Data sent (Resend): %d\n", list[min_index].pkt_data.channel_no);
            printf("RESENT  : Seq No. %-4d from %-6s to %-6s of size %-3d\n", list[min_index].pkt_data.seq_no, "CLIENT", dest[list[min_index].pkt_data.channel_no], list[min_index].pkt_data.size);
            record_log("CLIENT", "RE", get_current_time(), "DATA", list[min_index].pkt_data.pkt_no, list[min_index].pkt_data.seq_no, "CLIENT", dest[list[min_index].pkt_data.channel_no]);
        }

        else {
            // in case ack is received
            PACKET rcv_pkt;
            struct sockaddr_in si_rec;
            int slen = sizeof(si_rec);

            int received = recvfrom(sock, &rcv_pkt, sizeof(rcv_pkt), 0, (struct sockaddr *) &si_rec, &slen);
            if(received == -1) {
                die("ACK Receive Error");
            }
            // printf("ACK received: %d\n", rcv_pkt.channel_no);
            printf("RCVD ACK: Seq No. %-4d from %-6s to %-6s\n", rcv_pkt.seq_no, dest[rcv_pkt.channel_no], "CLIENT");
            record_log("CLIENT", "R", get_current_time(), "ACK", rcv_pkt.pkt_no, rcv_pkt.seq_no, dest[rcv_pkt.channel_no], "CLIENT");
            int s_no = rcv_pkt.sr_seq;

            // in case duplicate ack is received, ignore
            if(is_valid[s_no] && list[s_no].pkt_data.seq_no != rcv_pkt.seq_no) {
                continue;
            }

            is_valid[s_no] = 0;
            acked[s_no] = 1;

            // if last packet sent, no more packet to be sent
            // if received packet's ACK is not the expected ACK, then we do not shift window and send new packet
            if(quit || s_no != expected_ack) {
                continue;
            }

            // else if received packet ack is equal to expected ack
            // shift window and send packets until next unacked packet
            while(acked[expected_ack] == 1) {
                acked[expected_ack] = 0;

                // if no more packets to be sent
                if(quit) {
                    expected_ack += 1;
                    if(expected_ack == ws)
                        expected_ack = 0;
                    continue;
                }

                PACKET send_pkt;
                int bytes_read = load_data(fp, send_pkt.data, PACKET_SIZE);
                if(bytes_read == 0) {
                    send_pkt.is_last = 1;
                    quit = 1;
                }
                else
                    send_pkt.is_last = 0;
                send_pkt.channel_no = 1 - (j % 2);
                send_pkt.seq_no = k;
                send_pkt.type = 0;
                send_pkt.size = bytes_read;
                send_pkt.sr_seq = available_slot;
                send_pkt.pkt_no = j+1;
                acked[available_slot] = 0;
                is_valid[available_slot] = 1;

                // initialize the time variables
                list[available_slot].pkt_data = send_pkt;
                gettimeofday(&list[available_slot].start_time, 0);
                list[available_slot].timeout.tv_sec = TIMEOUT;
                list[available_slot].timeout.tv_usec = 0;
                timeradd(&list[available_slot].timeout, &list[available_slot].start_time, &list[available_slot].end_time);

                available_slot += 1;
                if(available_slot == ws)
                    available_slot = 0;
                expected_ack += 1;
                if(expected_ack == ws)
                    expected_ack = 0;

                k += bytes_read;
                j += 1;

                int sent = sendto(sock, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr *) &(si_relay[send_pkt.channel_no]), sizeof(si_relay[send_pkt.channel_no]));
                if(sent != sizeof(send_pkt))
                    die("Packet Sending Error");
        
                // printf("Data Sent: %d\n", send_pkt.channel_no);
                printf("SENT PKT: Seq No. %-4d from %-6s to %-6s of size %-3d\n", send_pkt.seq_no, "CLIENT", dest[send_pkt.channel_no], send_pkt.size);
                record_log("CLIENT", "S", get_current_time(), "DATA", send_pkt.pkt_no, send_pkt.seq_no, "CLIENT", dest[send_pkt.channel_no]);
            }
    
        }

    }

    printf("Closing Client\n");
    fclose(fp);
    close(sock);
    
    return 0;
}
