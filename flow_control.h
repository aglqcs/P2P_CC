#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include <time.h>
#include "packet.h"
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_PACKET_NUMBER 512

#define UNSEND 1
#define UNACKED 2
#define ACKED 3

#define SLOWSTART 1
#define CON_AVOID 2
#define FAST_RETRANSMIT 3

#define TRUE 1
#define FALSE 0

#define TIMEOUT 3

typedef struct data{
	int sockfd;
	char *content; // 512 * 1024 byte data
	char state[CHUNK_PACKET_NUMBER];
	int send_window;
 	int ssthresh;
  	char congestion_control_state;
    float increase_rate;
    u_int prev_ack;
    int count_ack;
  	int start;
  	int end;
} data_t;  


typedef struct data_list{
	data_t *data;
	struct data_list *next;
} data_list_t;



typedef struct data_chunk{
	int acked;
	int recved;
	int offset;
	char content[1024];
} data_chunk_t;


typedef struct recv_buffer{
	int sockfd;
	int expected;
	data_chunk_t chunks[CHUNK_PACKET_NUMBER];
} recv_buffer_t;

typedef struct recv_buffer_list{
	recv_buffer_t *buffer;
	struct recv_buffer_list *next;
}recv_buffer_list_t;

// This is use to track every packet you send and use for flow control
typedef struct packet_tracker{
    time_t send_time;
    data_packet_t *packet;
    struct packet_tracker *next;
} packet_tracker_t;



packet_tracker_t* create_timer(packet_tracker_t *p_tracker, data_packet_t *packet);
packet_tracker_t* find_packet_tracker(packet_tracker_t* p_tracker, data_packet_t* packet);
int discard_tracker(packet_tracker_t* list, packet_tracker_t* object);
void congestion_control(data_list_t* d);
void process_packet_loss(data_list_t *d);
int check_return_ack(data_list_t* d, data_packet_t* packet);

#endif

