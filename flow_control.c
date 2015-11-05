/*
	TODO:  maintain data structure for chunk and implement sliding window and flow control
*/
#include <stdio.h>
#include <stdlib.h>
#include "flow_control.h"
#include "packet.h"

data_list_t *list = NULL;
recv_buffer_list_t *recv_list = NULL;


/*
	start functions for sender side
*/
void init_datalist(int sockfd, char *content){
	/* this function receive the data from packet.c(from master data file) and separate them into chunks */
	data_list_t *new_element = (data_list_t *)malloc(sizeof(data_list_t));
	new_element->data->sockfd = sockfd;
	new_element->data->send_window = 1;	

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

data_t* get_data_by_sockfd(int sockfd){
	data_list_t *p;
	for( p = list; p != NULL; p = p->next){
		int find = 1;
		int i;
		for( i = 0; i < 20; i ++){
			if( sockfd != p->data->sockfd) {
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

data_packet_list_t* send_data(int sockfd){
	data_t *to_send = get_data_by_sockfd(sockfd);

	data_packet_list_t* ret = NULL;
	if( to_send == NULL ){
		printf("in send_data(), can not locate data_t for hash [%d]\n", sockfd);
		return NULL;
	}

	/* calculate the diff(current window size) for this data */
	int diff = to_send->end - to_send->start;

	/* if diff less than max window size, send packets */
	if( diff < to_send->send_window  && to_send->end <=  CHUNK_PACKET_NUMBER - 1 ){
		int i;
		for( i = 0;i < to_send->send_window - diff; i ++){
			
			/* make sure we do not reach the outside of the available data*/
			if( to_send->end >= CHUNK_PACKET_NUMBER - 1){
				printf("last packet already sent\n");
				return ret;
			}

			to_send->end ++;


			char *content = get_content_by_index(to_send, to_send->end);
			data_packet_t *packet = init_packet('3', content);

			/* notice that write the seq number here */
			/* notice that I do not change to the network bit sequence */
			packet->header.seq_num = to_send->end;
			to_send->state[to_send->end] = UNACKED;

			/* add the packet to the list */
			if ( ret == NULL ){
  				ret = (data_packet_list_t *)malloc( sizeof(data_packet_list_t));
  				ret->packet = packet;
  				ret->next = NULL;
  			}
  			else{
 				data_packet_list_t *new_block = (data_packet_list_t *)malloc( sizeof(data_packet_list_t));
  				new_block->packet = packet;
  				new_block->next = ret;
  				ret = new_block;
  			}
		}
	}
	return ret;
}

data_packet_list_t* handle_ack(int sockfd , int ack_number){
	data_t *to_send = get_data_by_sockfd(sockfd);
	if( to_send == NULL ){
		printf("in handle_ack(), can not locate data_t for hash [%d]\n", sockfd);
		return NULL;
	}
	int i;
	for(i = to_send->start; i <= ack_number; i ++){
		if( to_send->state[i] != UNACKED){
			printf("Should never happens, a packet not even send but got acked\n");
			return NULL;
		}
		to_send->state[i] = ACKED;
	}
	to_send->start = ack_number;

	/* Since the window size changed, we have a possibility to send more data, so call send_data() here */
	return send_data(sockfd);
}


/*
	end functions for sender side
	start functions for recver side
*/

void init_recv_buffer(int sockfd, char *hash){
	int i;

	recv_buffer_list_t *new_element = (recv_buffer_list_t *)malloc( sizeof(recv_buffer_list_t));
	new_element->buffer = (recv_buffer_t *)malloc( sizeof(recv_buffer_t));

	new_element->buffer->sockfd = sockfd;
	new_element->buffer->expected = 0;
	new_element->buffer->hash = malloc(20);
	for(i = 0;i < 20; i ++){
		new_element->buffer->hash[i] = hash[i];
	}

	for(i = 0; i < CHUNK_PACKET_NUMBER; i ++){

		new_element->buffer->chunks[i].acked = FALSE;
		new_element->buffer->chunks[i].recved = FALSE;
	} 

	if( recv_list == NULL ){
		new_element->next = NULL;
		recv_list = new_element;
	}
	else{
		new_element->next = recv_list;
		recv_list = new_element;
	}
}


data_packet_list_t* recv_data(data_packet_t *packet, int sockfd){
	int seq = packet->header.seq_num;
	recv_buffer_list_t *head;
	data_packet_list_t *ret = NULL;
	int find = 0;
	for( head = recv_list ; head != NULL; head = head->next ){
	// loop all the list and find the correct data buffer
		if( head->buffer->sockfd == sockfd ){
			find = 1;
			int i;

			/* return an ack with ack = seq, and mark the 'acked' and 'recved', store the data*/
			
			/* if never saw this part before, write it to memory*/
			if( head->buffer->chunks[seq].recved == FALSE){
				head->buffer->chunks[seq].recved = TRUE;
  				for(i = 0;i < 1024; i ++){
  					head->buffer->chunks[seq].content[i] = packet->data[i];
  				}
			}


			data_packet_t *packet = init_packet(4, NULL);

			/* loop the data structure and find the highest continue "recved" data */
			for(i = head->buffer->expected; i < 1024; i ++){
				if( head->buffer->chunks[i].recved != TRUE ){
					break;
				}
				else{
					head->buffer->chunks[seq].acked = TRUE;
				}
			}
			/* now i-1 is the last recved data */
			/* return ack is i-1 and expected to see i*/
			packet->header.ack_num = i - 1;
			head->buffer->expected = i;
			if ( ret == NULL ){
  				ret = (data_packet_list_t *)malloc( sizeof(data_packet_list_t));
  				ret->packet = packet;
  				ret->next = NULL;
  			}
  			else{
 				data_packet_list_t *new_block = (data_packet_list_t *)malloc( sizeof(data_packet_list_t));
  				new_block->packet = packet;
  				new_block->next = ret;
  				ret = new_block;
  			}
  			
  			break;
		}
	}
	if( find == 0){
		printf("can not locate the recv buffer.\n");
		return NULL;
	}
	return ret;
}

/* helper functions */
recv_buffer_t *get_buffer_by_hash(char *hash){
	recv_buffer_list_t *head;
	for( head = recv_list ; head != NULL; head = head->next ){
		int find = 1;
		int i;
		for(i  = 0; i < 20; i ++){
			if( head->buffer->hash[i] != hash[i]){
				find = 0;
				break;
			}
		}
		if( find == 1){
			return head->buffer;
		}
	}
	return NULL;
}