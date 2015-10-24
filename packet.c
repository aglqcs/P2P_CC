#include "packet.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bt_parse.h"


/*
for convenient show the structrue here

typedef struct header_s {
  short magicnum;
  char version;
  char packet_type;
  short header_len;
  short packet_len; 
  u_int seq_num;
  u_int ack_num;
} header_t;  

typedef struct data_packet {
  header_t header;
  char data[100];
} data_packet_t;
*/

int read_chunkfile(char * chunkfile, char *data){
	FILE *fp = fopen( chunkfile, "r");
  	if( fp == NULL ){
    	printf("%s can not be found\n", chunkfile);
    	return -1;
  	}
  	int index = 0;
  	char line[40];
  	memset(line, 0, 40);
  	int count = 0;
  	while( fscanf(fp, "%d %s\n", &index, line)  > 0 ){
  		/* the max size of this line should less then 40 */
  		if( strlen( line ) > 40 )
  			return -1;
  		int i;
  		for(i = 0;i < 40;i += 2){
  			data[count + i / 2] = line[i] << 4 | line[i + 1];
  		}
  		count += 20;
  		memset(line, 0, 40);

  	}
  	data[count] = '\0';
	return count;  	
}
data_packet_t *init_packet(char type, char *data){
	printf("init_packet()\n");
	/* if data length larger than 100, return a null pointer */
	unsigned int data_length;
	if( (data_length = strlen(data)) > 100){
		return NULL;
	}

	data_packet_t *packet = (data_packet_t *) malloc( sizeof(data_packet_t));
	memset( packet, 0, sizeof(data_packet_t));

	packet->header.magicnum = 15441;
	packet->header.version = 1;
	packet->header.packet_type = type;
	packet->header.header_len = 16;
	packet->header.packet_len = data_length + 16;

	/* if type == WHOHAS || IHAVE , ignore the seq_num and ack_num */
	if( type == '0' || type == '1'){
	}
	else{
		// else write the two fields
	}

	/* write the data */

	/* if type == WHOHAS || IHAVE , write the count of hashes and do the padding */
	if( type == '0' || type == '1'){
		char padding[4];
		padding[0] = data_length / 20;
		padding[1] = padding[2] = padding[3] = 1;
		strcat(packet->data, padding);
		memcpy( &packet->data[4], data, strlen(data));
	
		packet->header.packet_len += 4;
	}
	else{
		// write datafield for other packet type
	}

	return packet;
}

data_packet_t *build_packet_from_buf(char *buf){
	/* in theory this should be correct but if not correct we can modify this */
	return (data_packet_t *)(buf);
}

int find_in_local_has(char *hash, char *local){
	int local_has_size = strlen(local) / 20;

	int i;
	for(i = 0;i < local_has_size; i ++){
		int find = 1;
		int j;
		for(j = 0; j < 20; j ++){
			if( hash[j] != local[i * 20 + j]){
				find = -1;
				break;
			}
		}
		if( find == 1){
			return 1;
		}
	}
	return 0;
}


data_packet_t *handle_packet(data_packet_t *packet, bt_config_t* config){
	if(packet->header.packet_type == '0'){
		/* if incoming packet is a WHOHAS packet */
		/* scan the packet datafiled to fetch the hashes and get the count */
		int count = packet->data[0];

		int i;
		char local_has[100];
		char data[100];
		memset(data, 0 ,100);
		int reply_count = 0;

		char * chunkfile = config->has_chunk_file;
		if ( read_chunkfile(chunkfile, local_has) < 0 ){
			printf("Can not locate local chunkfile = %s\n", chunkfile);
			return NULL;
		}

		for(i = 0;i < count; i ++){
			int hash_start = 4 + 20 * i;

			int j;
			char request_chunk[20];
			for(j = hash_start; j <  hash_start + 20; j ++){
				/* if I have the chunck locally */
				request_chunk[j - hash_start] = packet->data[j];
			}
			if( 1 == find_in_local_has(request_chunk, local_has) ){
				printf("find a valid local hash [%s]\n", request_chunk);
				strcat(data, request_chunk);
				reply_count += 20;
			}
		}
		data[reply_count] = '\0';
		return init_packet('1', data);

	}
	else{
		/* TODO(for next checkpoint) : if incoming packet is other packets*/
	}
	return NULL;
}

data_packet_list_t *generate_WHOHAS(char *chunkfile){
  	data_packet_list_t *ret = NULL;

  	char data[1200];
  	FILE *fp = fopen( chunkfile, "r");
  	if( fp == NULL ){
    	printf("%s can not be found\n", chunkfile);
    	return NULL;
  	}
  	int index = 0;
  	char line[40];
  	memset(line, 0, 40);
  	int count = 0;
  	while( fscanf(fp, "%d %s\n", &index, line)  > 0 ){
  		/* the max size of this line should less then 40 */
  		if( strlen( line ) > 40 )
  			return NULL;
  		int i;
  		for(i = 0;i < 40;i += 2){
  			data[count + i / 2] = line[i] << 4 | line[i + 1];
  		}
  		count += 20;
  	
  		memset(line, 0, 40);

  		if( count >= 1000 ){
  			data[count] = '\0';
  			data_packet_t *packet = init_packet('0',  data);
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
  			count = 0;
  			memset(data, 0 , 1200);
  		}
  	}

  	data[count] = '\0';
  	data_packet_t *packet = init_packet('0',  data);
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
  	return ret;
}

