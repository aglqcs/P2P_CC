/*
	TODO:  maintain data structure for chunk and implement sliding window and flow control
*/

	
#include <stdio.h>
#include <stdlib.h>
#include "flow_control.h"
#include "packet.h"
#include <time.h>
#include <sys/socket.h>


#include <unistd.h>
data_list_t *list = NULL;
recv_buffer_list_t *recv_list = NULL;

void process_packet_loss(int offset){
	data_list_t *d;
	for( d = list; d != NULL; d = d->next){
		if( d->data->offset == offset ) break;
	}

	if( d == NULL ){
		return;
	}

    d->data->ssthresh = d->data->send_window/2;
    d->data->send_window = 1;
    d->data->congestion_control_state = SLOWSTART;
    d->data->increase_rate = 1;
}

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
	new_element->data->increase_rate = 0;

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


/* 
 * Check if return packet is duplicate,
 * if not, change prev_ack to newest_return ack
 * else change congestion statsu to FAST_RETRANSMIT
 * If duplicate ack num is 3, send the packet again.
 */

int check_return_ack(data_t* data, int ack_num){
    if(data->prev_ack == ack_num){
        data->count_ack += 1;
        if(data->count_ack >= 2)
            return 1;
    }
    else{
        data->count_ack = 0;
        data->prev_ack = ack_num;
    }
    return 0;
}

void congestion_control(data_t* data){
    if(data == NULL){
        return;
    } 
    if(data->congestion_control_state == SLOWSTART){
        // sending window reaches max value
        if(data->ssthresh == data->send_window){
            data->congestion_control_state = CON_AVOID;
            data->increase_rate = 0;
        }
        else{
            data->send_window += 1;
        }
    }
    else if(data->congestion_control_state == CON_AVOID){
        float cur_rate = 1/(float)data->send_window;
        data->increase_rate += cur_rate;
        if(data->increase_rate >= 1){
            data->send_window += 1;
            data->increase_rate = 0;
        }
    }
}
/*

packet_tracker_t* remove_from_tracker(packet_tracker_t* current, int offset, int seq){
	// copy from http://www.ardendertat.com/2011/09/29/programming-interview-questions-5-linkedlist-remove-nodes/
	
	if (current == NULL) return NULL;

	if (current->packet->header.seq_num == seq && current->packet->header.ack_num == offset){
		current = current->next;
		current = remove_from_tracker(current, offset,seq);
		}
	else current->next = remove_from_tracker(current->next, offset, seq);

	return current;
}
*/
int remove_from_tracker(packet_tracker_t* current, int offset, int seq){
	if( current == NULL ) return -1;
	if( current->packet->header.seq_num == seq && current->packet->header.ack_num == offset){
		*current = *current->next;
		return 1;
	}
	packet_tracker_t* prev = current;
	packet_tracker_t* p = current->next;
	while( p != NULL){
		if( p->packet->header.seq_num == seq && p->packet->header.ack_num == offset){
			prev->next = p->next;
			return 1;			
		}
		p = p->next;
		prev = prev->next;
	}
	return -1;
}

data_packet_list_t* handle_ack(int offset , int ack_number, packet_tracker_t *p_tracker){
	data_t *to_send = get_data_by_offset(offset);
	if( to_send == NULL ){
		printf("in handle_ack(), can not locate data_t for offset [%d]\n", offset);
		return NULL;
	}
	if( 1 == check_return_ack(to_send, ack_number) ){
		/* 3 duplicate fast re-transmit */
		printf("3 Duplicate ACK = %d\n", ack_number);
		char *content = get_content_by_index(to_send, ack_number + 1);
		data_packet_t *packet = init_packet(3, content, 1024);
		/* dont forget write seq and offset*/
		packet->header.seq_num = ack_number + 1;
		packet->header.ack_num = offset;
		process_packet_loss(offset);

		data_packet_list_t* ret = (data_packet_list_t *)malloc( sizeof(	data_packet_list_t));
		ret->next = NULL;
		ret->packet = packet;
		return ret;
	}

	int i;

	for(i = to_send->start + 1; i <= ack_number; i ++){
		if( to_send->state[i] == UNSEND){
			printf("Should never happens, a packet not even send but got acked\n");
			return NULL;
		}
		if( to_send->state[i] != ACKED ){
			to_send->state[i] = ACKED;
			/* remove from p_trackerlist */
			printf("KKKKKKK WILL DELETE %d\n",i);
			printf("~~~~~~~~~~~~~~~~~~\n");

			 packet_tracker_t *p = p_tracker;
			 while( p != NULL ){
			 	printf("%d - " , p->packet->header.seq_num);
			 	p=p->next;
			 }
			 printf("\n~~~~~~~~~~~~~~~~~~\n");
			 if( -1 == (remove_from_tracker(p_tracker, offset, i))){
			 	printf("TRY to discard %d fail\n", i);
			 }
			 else{
			 	printf("TRY to discard %d SUCCESS\n", i);

			 }
			 	printf("~~~~~~~~~~~~~~~~~~\n");

			  p = p_tracker;
			 while( p != NULL ){
			 	printf("%d - " , p->packet->header.seq_num);
			 	p=p->next;
			 }
			 printf("\n~~~~~~~~~~~~~~~~~~\n");
			// if( NULL == (	p_tracker = remove_from_tracker(p_tracker, offset, i) )){
			// 	printf("Unable to delete from p_tracker with offset = %d, seq = %d\n", offset, i);
			// 	return NULL;
			// }
			// else{
			// 	printf("-----------------\nDELETE from p_tracker with offset = %d, seq = %d\n----------------\n", offset, i);
			// }
		}
		else{
			// it may be a duplicate message, do nothing
		}
	}
	to_send->start = ack_number;

	congestion_control(to_send);

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

		/*	if( seq > head->buffer->expected ){
				// if receive a larger seq number than expected packet DO NOT send ack
				return NULL;
			}
			*/

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


/* Timer control */

packet_tracker_t* create_timer(packet_tracker_t *p_tracker, data_packet_t *packet, int sock, struct sockaddr_in *from){
    printf("Create TIMEer with seq = %d\n", packet->header.seq_num);
    if(p_tracker == NULL){
        p_tracker = (packet_tracker_t *)malloc(sizeof(packet_tracker_t));
        p_tracker->send_time = time(NULL);
        p_tracker->packet = packet;
        p_tracker->next = NULL;
        p_tracker->sock = sock;
        p_tracker->from = from;
        p_tracker->expire_time = 0;

        /* dummy head */
        packet_tracker_t *itr = (packet_tracker_t *)malloc(sizeof(packet_tracker_t));
        itr->packet = (data_packet_t *)malloc(sizeof(data_packet_t));
        itr->packet->header.seq_num = -441;

        itr->next = p_tracker;
        p_tracker = itr;
        return p_tracker;
    }
    else{
        packet_tracker_t *itr = p_tracker;
        packet_tracker_t *pre = NULL;

        int offset = packet->header.ack_num;
        int seq = packet->header.seq_num;
        while(itr != NULL){
            if( itr->packet->header.seq_num == seq && itr->packet->header.ack_num == offset){
                printf("Timer for packet = %d already exist\n", seq);
                itr->send_time = time(NULL);
                return p_tracker;
            }
            pre = itr;
            itr = itr->next;
        }
        // Create new packet_tracker
        itr = (packet_tracker_t *)malloc(sizeof(packet_tracker_t));
        itr->send_time = time(NULL);
        itr->packet = packet;
        itr->next = NULL;
        itr->sock = sock;
        itr->from = from;
        itr->expire_time = 0;
        pre->next = itr;

        return p_tracker;
    }
}

int timer_expired(packet_tracker_t *p_tracker){
    time_t current_time = time(NULL);
    double diff_t;
    diff_t = difftime(current_time, p_tracker->send_time);
       // printf("In timer: seq = %d send time = %lld, now = %lld diff =%lld \n",p_tracker->packet->header.seq_num, (long long int)p_tracker->send_time, (long long int)current_time,(long long int)diff_t );

    if(diff_t >= TIMEOUT){
        p_tracker->expire_time += 1;
        return -1;
    }
    return 0;
}


/*
void process_packet_loss(data_list_t *d){
    d->data->ssthresh = d->data->send_window/2;
    d->data->send_window = 1;
    d->data->congestion_control_state = SLOWSTART;
    d->data->increase_rate = 1;
}*/