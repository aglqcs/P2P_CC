/*
	TODO:  maintain data structure for chunk and implement sliding window and flow control
*/
#include <stdio.h>
#include <stdlib.h>
#include "flow_control.h"
#include "packet.h"

data_list_t *list;

void init_datalist(char *hash, char *content){
	/* this function receive the data from packet.c(from master data file) and separate them into chunks */
	data_list_t *new_element = (data_list_t *)malloc(sizeof(data_list_t));
	new_element->data->hash = hash;
	new_element->data->content = content;
	
	/* init the congestion control variable */
	new_element->data->send_window = 1;
	new_element->data->ssthresh = 8;
	new_element->data->congestion_control_state = SLOWSTART;
	new_element->data->start = new_element->data->end = -1;

	int i;
	for( i = 0; i < CHUNK_PACKET_NUMBER; i ++){
		new_element->data->state[i] = UNSEND;
	}
	if( list == NULL ){
		new_element->next = NULL;
		list = new_element;
	}
	else{
		new_element->next = list;
		list = new_element;
	}
}

char* get_content_by_index(data_t *data, int index){
	char *content = malloc(1024);
	int i;
	int start = index * 1024;
	for(i = 0; i < 1024; i ++){
		content[i] = data->content[start + i];
	}
	return content;
}

data_t* get_data_by_hash(char *hash){
	data_list_t *p;
	for( p = list; p != NULL; p = p->next){
		int find = 1;
		int i;
		for( i = 0; i < 20; i ++){
			if(hash[i] != p->data->hash[i]) {
				find = -1;
				break;
			}
		}
		if(find == 1){
			return p->data;
		}
	}
	return NULL;
}

void send_data(char *hash){
	data_t *to_send = get_data_by_hash(hash);
	if( to_send == NULL ){
		printf("in send_data(), can not locate data_t for hash [%s]\n", hash);
		return;
	}

	/* calculate the diff(current window size) for this data */
	int diff = to_send->end - to_send->start;

	/* if diff less than max window size, send packets */
	if( diff < to_send->send_window ){
		int i;
		for( i = 0;i < to_send->send_window - diff; i ++){
			to_send->end ++;
			char *content = get_content_by_index(to_send, to_send->end);
			data_packet_t *packet = init_packet('3', content);

			/* notice that write the seq number here */
			/* notice that I do not change to the network bit sequence */
			packet->header.seq_num = to_send->end;
			to_send->state[to_send->end] = UNACKED;
			/* call spiffy_send() to send this data */
		}
	}
}

void handle_ack(char *hash , int ack_number){
	data_t *to_send = get_data_by_hash(hash);
	if( to_send == NULL ){
		printf("in handle_ack(), can not locate data_t for hash [%s]\n", hash);
		return;
	}
	to_send->state[ack_number] = ACKED;
	to_send->start = ack_number;
}