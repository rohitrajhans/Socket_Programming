# Computer Networks Assignment 1

## Steps to run:
1. Open folder Q1
2. run `make` to compile all the files
3. Ensure `input.txt` exists
4. On two separate terminal instances, run `./server` and `./client` respectively without any arguments
5. View the transferred content at `output.txt`
6. Edit macros in `packet.h` for testing and goto step 2


## Question (in brief):
1. Client-Server connected through TCP architecture.
2. Client can send packets to server through two poosible channels.
3. Server sends an ACK on the same channel on which it has received the packet.
4. Server buffers out-of-order packets on a limited buffer.


## Assumptions:
1. All the used macros are defined in `packet.h`
2. PACKET_SIZE = 100: This means that the size of payload (character array) is 100 bytes
3. Server Port number stored as a macro in `packet.h`
4. Sequence Number is the first byte stored in the packet. Example, if PACKET_SIZE is 100 then sequence number for packet 3 will be 200
5. Initially 1 packet is sent on each channel even if input.txt is empty
6. ACKs are not dropped
7. BUFFER_SIZE is a multiple of PACKET_SIZE


## Implementation:
- The client creates MAX_CLIENTS (macro in `packet.h`) number of sockets. These refer to the number of channels through which it can send data.
- The server creates a socket to listen for new connections and a new socket on every new connection for communication.
- On successful connection, client sends 1 packet each on both channels waiting for the server to acknowledge.
- Each packet has its separate timer.
- Corresponding to a packet, extra information is stored on client side. These are the time when packet was sent, and the remaining timeout value. 
    The initial timeout value is defined in the macro TIMEOUT in `packet.h`.
- The server immediately writes the data of an in-order packet to the file while it adds an out-of-order packet to the buffer and writes it after the in-order packet is received.
- The multiple timers problem is handled by maintaing a list of packets and their corresponding timeouts and always selecting the lowest timeout value for `select()`
- The `select()` function in `client.c` monitors the client socket descriptors and has a timeout value equal to the minimum of the timeout values of all the sent but unacknowledged packets.
- A packet drop occurs through 2 scenarios. A random packet drop is implemented at the server, each channel has a separate drop rate.
    In case the buffer is full, a new incoming out-of-order packet is dropped by the server.
- The `select()` function in `server.c` monitors three socket descriptors for incoming packets and new incoming connections.
- In case of a timeout/ packet drop, new packets keep transmitting through the free channel. Dropped packet is resent with a new timeout value.
    Sent but unacknowledged packets are stored on the `client` until they are acknowledged so that they can be resent in case of packet loss.
- Server closes itself on receiving all the packets. Client closes itself on receiving ACK for all the packets.
- Macros can be modified for testing.
- The number of channels can also be modified for testing (Code is generalized to handle it. Some additional macros will be needed for packet drop conditions of new channels).
