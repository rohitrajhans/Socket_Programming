# Computer Networks Assignment 2

## Steps to run:
1. Open folder Q2
2. run `make` to compile all the files
3. Ensure `input.txt` exists
4. Open four separate terminal instances
5. run `./server` to start the server
6. run `./relay 0` where 0 is a command line argument to start relay node 0
7. run `./relay 1` where 1 is a command line argument to start relay node 1
8. run `./client` to start the client
9. View logs at `log.txt`
10. View the transferred content at `output.txt`
11. Edit macros in `packet.h` for testing and goto step 2


## Question (in brief):
1. Client and Server connected through UDP architecture.
2. Communication takes place through two different relay nodes.
3. Even-numbered packets go through relay node 0, Odd-numbered packets go through relay node 1
4. Relay nodes can add a delay between 0 and 2ms (defined as a macro in `packet.h`)


## Assumptions:
1. All macros are defined in `packet.h`
2. Window Size for client is BUFFER_SIZE + 1 (server). This is because at most BUFFER_SIZE out-of-order packets can be buffered at server.
    So we can send BUFFER_SIZE + 1 packets without the buffer being full. WINDOW_SIZE is a macro, and can be modified.
3. Since, the relay nodes are also running on the same machine, they have different port numbers for differentiation defined as a macro.
4. Server sends an ACK as soon as it receives a data packet.
5. ACK packet cannot be dropped or delayed at a relay node.
6. Sequence Number (seq_no) is the first byte stored in the packet. Example, if PACKET_SIZE is 100 then sequence number for packet 3 will be 100
    Whereas, Packet Number (pkt_no) stores the packet number. Both are printed in `log.txt`, since question did not clearly specify what the sequence number was.
7. Each packet has a separate timer.
8. Server buffer stores the entire packet structure.
9. BUFFER_SIZE is a multiple of PACKET_SIZE.


## Implementation:
- The client, each relay node and the server have one socket each for communication.
- A single socket is sufficient since the underlying protocol is UDP.
- Client initially sends WINDOW_SIZE number of packets to server through different relay nodes.
- The multiple timers problem is handled by maintaing a list of packets and their corresponding timeouts and always selecting the lowest timeout value for `select()`.
- Each packet has a separate timeout. The `select()` function in the client monitors the client socket and has a timeout value equal to the minimum timeout of all the sent but unacknowledged packets.
    The timeout values of all packets are periodically updated.
- A relay node can drop a packet based on a randomly generated number and packet drop rate mentioned in `packet.h`. It also adds a delay.
- The server sends an ACK on successfully receiving a data packet through the same relay node. ACK packets cannot be dropped or delayed.
- The server writes the data of an in-order packet directly to the file `output.txt` while it buffers an out-of-order packet.
- The client maintains a boolean array `acked[]`. It is true if acknowledgement for packet with SR sequence number i has been acknowledged.
    This array is helpful since acknowledgements can also be out-of-order. This prevents retransmission of an already ACKed packet.
- The client only shifts its window in case the expected ACK is received.
- The client sends the next packet when it receives or has already received the expected ACK.
- The server closes itself once it has received all the packets and `input.txt` has been completely transferred.
- The client closes itself once it has received an ACK for all its packets and `input.txt` has been completely transferred.
- The relay nodes do not know when the transfer is complete. So they are closed if there is a fixed amount of inactivity in the network.
    The amount has been set to 15 seconds. It can be modified.
