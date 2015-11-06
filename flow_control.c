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
void init_datalist(int offset, char *content){
	/* this function receive the data from packet.c(from master data file) and separate them into chunks */
	data_list_t *new_element = (data_list_t *)malloc(sizeof(data_list_t));
	new_element->data = (data_t *)malloc(sizeof(data_t));
	new_element->data->content = content;

	new_element->data->offset = offset;

	/* init the congestion control variable */
	new_element->data->send_window = 4;
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

data_t* get_data_by_offset(int offset){
	data_list_t *p;
	for( p = list; p != NULL; p = p->next){
		if( offset == p->data->offset){
			return p->data;
		}
	
	}
	return NULL;
}

data_packet_list_t* send_data(int offset){
	data_t *to_send = get_data_by_offset(offset);

	data_packet_list_t* ret = NULL;
	if( to_send == NULL ){
		printf("in send_data(), can not locate data_t for offset [%d]\n", offset);
		return NULL;
	}

	/* calculate the diff(current window size) for this data */
	int diff = to_send->end - to_send->start;

	/* if diff less than max window size, send packets */
	printf("DEBUG: Call send_data, can send packet = %d, start = %d, end = %d, window = %d\n", to_send->send_window - diff,to_send->start, to_send->end, to_send->send_window);
	
	//if( diff < to_send->send_window  && to_send->end <=  8 ){
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
			data_packet_t *packet = init_packet(3, content, 1024);
			printf("SEND DATA seq = %d, offset = %d\n", to_send->end, offset);
			/* notice that write the seq number here */
			/* notice that I do not change to the network bit sequence */
			packet->header.seq_num = to_send->end;
			to_send->state[to_send->end] = UNACKED;

			/* notice that I need to know which hash chunk of the data belong to, so I add to ACK field*/
			packet->header.ack_num = offset;

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
	printf("DEBUG : send over window start = %d window end = %d\n", to_send->start, to_send->end);
	return ret;
}

data_packet_list_t* handle_ack(int offset , int ack_number){
	data_t *to_send = get_data_by_offset(offset);
	if( to_send == NULL ){
		printf("in handle_ack(), can not locate data_t for offset [%d]\n", offset);
		return NULL;
	}
	int i;
	for(i = to_send->start + 1; i <= ack_number; i ++){
		if( to_send->state[i] != UNACKED){
			printf("Should never happens, a packet not even send but got acked\n");
			return NULL;
		}
		to_send->state[i] = ACKED;
	}
	to_send->start = ack_number;
	printf("DEBUG : recv Ack over, window start = %d window end = %d\n", to_send->start, to_send->end);
	/* Since the window size changed, we have a possibility to send more data, so call send_data() here */
	return send_data(offset);
}


/*
	end functions for sender side
	start functions for recver side
*/

void init_recv_buffer(int offset){
	int i;
	printf("DEBUG initing offset = %d\n", offset);
	recv_buffer_list_t *new_element = (recv_buffer_list_t *)malloc( sizeof(recv_buffer_list_t));
	new_element->buffer = (recv_buffer_t *)malloc( sizeof(recv_buffer_t));

	new_element->buffer->expected = 0;
	new_element->buffer->offset = offset;
	/*for(i = 0;i < 20; i ++){
		new_element->buffer->hash[i] = hash[i];
	}*/

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


data_packet_list_t* recv_data(data_packet_t *recv_packet, int offset){
	int seq = recv_packet->header.seq_num;
	printf("RECV DATA seq = %d offset = %d\n", seq, offset);

	recv_buffer_list_t *head;
	data_packet_list_t *ret = NULL;
	int find = 0;
	for( head = recv_list ; head != NULL; head = head->next ){
	// loop all the list and find the correct data buffer
		if( head->buffer->offset == offset ){
			find = 1;
			int i;

			/* return an ack with ack = seq, and mark the 'acked' and 'recved', store the data*/
			
			/* if never saw this part before, write it to memory*/
			if( head->buffer->chunks[seq].recved == FALSE){
				head->buffer->chunks[seq].recved = TRUE;
  				for(i = 0;i < 1024; i ++){
  					head->buffer->chunks[seq].content[i] = recv_packet->data[i];
  				}
			}

			if( seq > head->buffer->expected ){
				/* if receive a larger seq number than expected packet DO NOT send ack*/
				return NULL;
			}

			data_packet_t *packet = init_packet(4, NULL, 0);
			/* notice that the sender should know the offset of the ACK, since seq is not used in packet*/
			packet->header.seq_num = offset;

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

			printf("DEBUG packet ack = %d, next expected = %d\n", packet->header.ack_num, head->buffer->expected);
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

recv_buffer_t *get_buffer_by_offset(int offset){
	recv_buffer_list_t *head;
	for( head = recv_list ; head != NULL; head = head->next ){
		if( offset == head->buffer->offset ){
			/* 	NOTE: important
				return head->buffer : ERROR!
				return &head->buffer : correct
			*/
				int i;
				for(i = 0;i < 512; i ++){
					if( head->buffer->chunks[i].recved == TRUE){
						printf("%d ",i);
					}
				}
				printf("\n");
			return (recv_buffer_t *)&head->buffer;
		}
	}
	return NULL;
}

void copy_chunk_data(char *buffer, int offset, int chunkpos){
	recv_buffer_list_t *head;
	for( head = recv_list ; head != NULL; head = head->next ){
		if( offset == head->buffer->offset ){
			int i;
			for(i = 0; i < 1024;i ++){
				// copy data here
				buffer[i] = head->buffer->chunks[chunkpos].content[i];
			}
		}
	}
}

int is_buffer_full(int offset){
	recv_buffer_list_t *head;
	int find = -1;
	for( head = recv_list ; head != NULL; head = head->next ){
		if( offset == head->buffer->offset ){
			find = 1;
			int i;
			for(i = 0;i < 512; i ++){
				if( head->buffer->chunks[i].recved == FALSE){
					printf("Offset = %d first missing chunks id = %d\n", offset,i);
					return -1;
				}
			}
		}
	}
	if ( find == -1)
		return -1;
	else
		return 1;
}