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

void die(char *msg) {
    perror(msg);
    exit(1);
}

// function to generate random number
double generate_random(int min, int max) {
    double r = (double)rand() / (double)((unsigned)RAND_MAX + 1);
    return (double)min + r * (double)(max - min);
}

// command line argument to specify relay node number
// Example: argument = 0: relay node 0 is run
// if argument = 1: relay node 1 is run
// Since relay node is running on same machine, it has to run on different PORTS
// Relay PORT nos are defined in `packet_def.h` as MACROS
// More number of relay nodes can be added just by adding port number as a macro and passing the relay node number as cmd line arg
int main(int argc, char* argv[]) {
    
    if(argc != 2) {
        printf("Error: Pass relay node (0 or 1) number as argument\n");
        printf("Sample Call to start relay node 0: ./relay 0\n");
        exit(0);
    }

    // get relay_node number
    int relay_node, port, pdr;
    relay_node = strtol(argv[1], (char**) NULL, 10);
    // log information
    char* relay_name[] = {"RELAY1", "RELAY2"};
    char* pkt_type[] = {"DATA", "ACK"};
    char* dest[] = {"SERVER", "CLIENT"};
    char* src[] = {"CLIENT", "SERVER"};

    // seed random number generator with current time
    srand(time(0));

    switch(relay_node) {
        case 0:
            port  = PORT_RELAY_1;
            pdr = PDR0;
            break;
        case 1:
            port  = PORT_RELAY_2;
            pdr = PDR1;
            break;
        default:
            printf("Only 2 relay nodes supported. Enter 0 or 1\n");
            exit(0);
            break;
    }

    // address structures
    struct sockaddr_in si_server, si_client, si_relay;
    int slen = sizeof(si_relay);
    
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

    // initialize relay address structure
    memset((char*) &si_relay, 0, sizeof(si_relay));
    si_relay.sin_family = AF_INET;
    si_relay.sin_port = htons(port);
    si_relay.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    int bind_bool = bind(sock, (struct sockaddr*) &si_relay, sizeof(si_relay));
    if(bind_bool == -1)
        die("Binding Error");
    printf("Binding Successful\n");

    printf("IP: %s, Port: %d\n", inet_ntoa(si_relay.sin_addr), (int)ntohs(si_relay.sin_port));

    // relay keeps running
    while(1) {
        struct sockaddr_in si_rcv;
        PACKET pkt;

        // if no I/O for 15 seconds then close relay node
        fd_set master;
        FD_ZERO(&master);
        FD_SET(sock, &master);
        struct timeval tv = {15, 0};
        int sleep_bool = select(sock + 1, &master, NULL, NULL, &tv);

        if(sleep_bool == -1)
            die("Select Error");
        else if(sleep_bool == 0)
            break;
            
        // for printing
        char* typ[] = {"PKT", "ACK"};

        int sent, received = recvfrom(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_rcv, &slen);
        if(received == -1)
            die("Receive Error");

        double r = generate_random(0, 1), prob = (double) pdr / 100;

        // only data packets are dropped
        if(r <= prob && pkt.type==0) {
            // printf("***Packet on relay node %d dropped***\n", relay_node);
            printf("DROP %-3s: Seq No. %-4d from %-6s to %-6s\n", typ[pkt.type], pkt.seq_no, src[pkt.channel_no] ,relay_name[relay_node]);
            record_log(relay_name[relay_node], "D", get_current_time(), "DATA", pkt.pkt_no, pkt.seq_no, "CLIENT", relay_name[relay_node]);
            continue;
        }

        printf("RCVD %-3s: Seq No. %-4d from %-6s to %-6s\n", typ[pkt.type], pkt.seq_no, src[pkt.channel_no] ,relay_name[relay_node]);
        record_log(relay_name[relay_node], "R", get_current_time(), pkt_type[pkt.type], pkt.pkt_no, pkt.seq_no, src[pkt.type], relay_name[relay_node]);

        // add random delay between 0 and DELAY ms only for data packet
        if(pkt.type == 0) {
            double delay = generate_random(0, DELAY);
            struct timeval tv_delay;
            tv_delay.tv_sec = 0;
            tv_delay.tv_usec = delay*1000; // ms
            // adding delay
            select(0, NULL, NULL, NULL, &tv_delay);
        }

        // if data packet then forward to server
        if(pkt.type == 0) {
            // store sender info in client address structure
            // this is necessary as we'll need client information to send packet to it
            memcpy(&si_client, &si_rcv, sizeof(si_rcv));

            sent = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_server, sizeof(si_server));
            if(sent == -1) {
                die("Sending Error");
            }
        }
        else if(pkt.type == 1) {
            // if ACK packet then forward to client
            sent = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr*) &si_client, sizeof(si_client));
        }

        if(sent == -1) {
            die("Sending Error");
        }

        // printf("Packet Forwarded from %s to %s: %d\n", str[pkt.type], str[1-pkt.type], pkt.seq_no);
        printf("SENT %-3s: Seq No. %-4d from %-6s to %-6s\n", typ[pkt.type], pkt.seq_no, relay_name[relay_node], dest[pkt.channel_no]);
        record_log(relay_name[relay_node], "S", get_current_time(), pkt_type[pkt.type], pkt.pkt_no, pkt.seq_no, relay_name[relay_node], dest[pkt.type]);
    }

    // close relay node
    printf("Closing Relay Node: %d\n", relay_node);
    close(sock);

    return 0;
}
