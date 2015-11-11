15-441 Computer Networks
Project2 Congestion Control with Bittorrent
Checkpoint2

Shuo Chen, Chun-Ning Chang

1. Checkpoint1 overview

Protocol implemented: WHOHAS, IHAVE, GET, DATA, ACK

Tasks: The network is implmented with sliding window and congestion control. The program will also guarantee 100% reliability. By detecting several packet loss, our server will determine that the node is dead and find another new node by flooding the whohas packet for partuclar chunk.



2. Code review(Brief discussion)

2.1 packet.c packet.h
In this file, we add several prototype of functions that may needed in Checkpoint1 and Checkpoint2.
Basically, for the file requester, we'll maintain a filemanager structure to keep track of all the chunks and only reconstruct the file while all the chunks are received.
While for the server whose responsible for transferring particular chunk, it'll generate the responsible IHAVE and if it's chosen for particular chunk, it'll try to send the packet within the chunk.
read_cunkfile() is a function to read a chunk file and get hash values.
init_packet() is a function to write the header of the packet, convert the header in to network byte order and fill the data field based on user data.
handle_packet() is a function that read a incoming packet and generate a response packet (Either IHAVE, DATA or ACK).

2.2 peer.c
In process_get(), after we parse the user command, we call the packet functions and use spiffy_send_to() to broadcast the packet to all the nodes available. In process_inbound_udp(), after we receive a packet, we call handle_packet() and generate a response then send back.
Besides these two steps, we also have a packet_tracker to track the current processing packet. This step can help us detect timeout, 
retransmit as well as congestion control(if packet loss time meet threshold).

2.3 flowcontrol.c
Most of the functions for sliding window, responsibility and congestion control are generated here.
process_packet_loss() is a function to change the congestion_control_state for particular chunk transmission to slow start while
detecting a packet loss.
data_list* related functions are use to maintain and record the particular chunk's recevied packet(receiver side), and packet send,
ack received, ssthres, sliding window size, congestion state( sender side).
For fast retransmit, we have a handle_ack() function to detect while 3 duplicates ack are returned. We'll then do a retranfer of that packet despite the timeout is not due yet.
Also for congestion control,  we'll also keep track of the increase rate. If the congestion state is SLOW_START, the window rate will increase by 1 for every ack received. While for CONGESTION_AVOIDANCE, the increase_rate will become 1/current_window_size when every 
ack is received.

2.4 log.c
This is use for graphing window size. We'll record only when the window size has changed as mention on the handout.
LOGOPEN() is for opening the record file, we'll save the file as problem2-peer.txt.
LOGCLOSE() will close the file after the prgram ends.
LOGIN() will record the chunk id, current time, and window size of the chunk transfer.

