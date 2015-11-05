15-441 Computer Networks
Project2 Congestion Control with Bittorrent
Checkpoint1

Shuo Chen, ChunNing Chang


1. Checkpoint1 overview

Protocol implemented: WHOHAS, IHAVE
Protocol not implemented: GET, DATA, ACK, DENIED

2. Code review(only show modifications)
2.1 packet.c packet.h
In this file, we add several prototype of functions that may needed in Checkpoint1 and later implementation. read_cunkfile() is a function to read a chunk file and get hash values. init_packet() is a function to write the header of the packet, convert the header in to network byte order and fill the data field based on user data. handle_packet() is a function that read a incoming packet and generate a response packet.

2.2 peer.c
In process_get(), after we parse the user command, we call the packet functions and use spiffy_send_to() to broadcast the packet to all the nodes available. In process_inbound_udp(), after we receive a packet, we call handle_packet() and generate a response then send back.

