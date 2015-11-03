#include "packet.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bt_parse.h"
#include "chunk.h"
#include <netinet/in.h>

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
/* An example chunk file is:
0 b129b015e8a4258dc8c0d0b3a63acaeda8d14cd8
1 8085b28ac91f81435f9108388a3a4c8d9b01e84a
*/
/* 	This read a chunkfile, convert the string hashvalue into binary value, and write into
	"data" parameter, return the size of the "data" parameter
*/
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
        hex2binary((char*)line, 40, (uint8_t *)(data + count));
  		count += 20;
  		memset(line, 0, 40);

  	}
  	data[count] = '\0';
    fclose(fp);
    return count;  	
}
data_packet_t *init_packet(char type, char *data){
/*	This function write the packet header and data and return a packet object
	notice that here I DO NOT write the seq and ack of the header
*/
	printf("init_packet() type = %c\ndata = %s\n", type, data);
	/* if data length larger than 100, return a null pointer */
	unsigned int data_length;
	if( data == NULL){
		data_length = 0;
	}
	else if( (data_length = strlen(data)) > 100){
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
	/* !! notice that I will not fill seq and ack here in this function but in the flow_control.c*/
	

	/* write the data */

	/* if type == WHOHAS || IHAVE , write the count of hashes and do the padding */
	if( type == 0 || type == 1){
		char padding[4];
		padding[0] = data_length / 20;
		padding[1] = padding[2] = padding[3] = 1;
		strcat(packet->data, padding);
		memcpy( &packet->data[4], data, strlen(data));
	
		packet->header.packet_len += 4;

	}
	else{
		// write datafield for other packet type
        memcpy(packet->data, data, strlen(data));
	}
    packet->header.magicnum = htons(packet->header.magicnum);
    packet->header.header_len = htons(packet->header.header_len);
    packet->header.packet_len = htons(packet->header.packet_len);
    packet->header.seq_num = htonl(packet->header.seq_num);
    packet->header.ack_num = htonl(packet->header.ack_num);
	return packet;
}

data_packet_t *build_packet_from_buf(char *buf){
	/* 	Normally after a recv() from network, this function will be called to convert a char
		buff into a data_packet_t object
	*/
	/* in theory this should be correct but if not correct we can modify this */
	return (data_packet_t *)(buf);
}

int find_in_local_has(char *hash, char *local){
	/*	search local hash file to find if there is a hash record that matches input
	*/
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

char* get_data_from_hash(char *hash , bt_config_t* config){
	/*	read the master data file and return the 512KB data for a chunk
		Two steps:
			(1) calculate the offset in the master chunk file based on hashvalue
			(2)	read the data form master data file based on the offset
	*/
	char data[512 * 1024];
	char temp[40];
	int index;
	char *master_file = config->chunk_file;
	FILE *fp = fopen(master_file, "r");
	if( fp == NULL ){
		printf("Can not locate file %s\n", hash);
		return NULL;
	}
	char content_path[512];
	fscanf(fp, "%s %s\n", temp, content_path);
	fscanf(fp, "%s\n", temp);
	int find = -1;
	while( find != 1 && fscanf(fp,"%d %s",&index, temp ) > 0){
		char local[20];

		/* calculate the binary hash from the master chunk file */
		hex2binary((char*)temp, 40, (uint8_t *)(local));

		int cmp = 1;
		int i;
		/* and compare if there is a match of hash values */
		for( i = 0;i < 20;i ++){
			if( local[i] != hash[i] ){
				cmp = 0;
				break;
			}
		}

		if( cmp == 1){
			printf("find local content for has = %s\n", local);
			find = 1;
		}

		memset(temp, 0, 40);
		memset(local, 0 ,20);
	}
	printf("in Master file offset = %d\n", index);
	FILE *content = fopen(content_path, "r");
	fseek(content, 512 * 1024 * index, SEEK_SET );
	fread(data,512 * 1024, 1, content);

	fclose(content);
	fclose(fp);
	return content;
}

data_packet_list_t *handle_packet(data_packet_t *packet, bt_config_t* config, struct sockaddr_in* addr, chunk_owner_list_t* c_list){
	/*	read a incoming packet, 
		return a list of response packets
	*/
	printf("handle_packet() type == %c\n", packet->header.packet_type);
	if(packet->header.packet_type == 0){
		/* if incoming packet is a WHOHAS packet */
		/* scan the packet datafiled to fetch the hashes and get the count */
		int count = packet->data[0];

		int i;
		char local_has[100];
		char data[1500];
		memset(data, 0 ,1500);
		int reply_count = 0;
        int find = 0;
		char * chunkfile = config->has_chunk_file;
		if ( read_chunkfile(chunkfile, local_has) < 0 ){
			printf("Can not locate local chunkfile = %s\n", chunkfile);
			return NULL;
		}
		int find = -1;

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
				find = 1;
				for( j = 0;j < 20;j ++){
					data[reply_count + j] = request_chunk[j];
				}
				reply_count += 20;
			}
		}
		data[reply_count] = '\0';
		
        if( find == -1){
			return NULL;
		}
		else{
			data_packet_list_t *ret = (data_packet_list_t *)malloc(sizeof(data_packet_list_t));
			ret->packet = init_packet(1, data);
			ret->next = NULL;
			return ret;
		}

	}
	else if( packet->header.packet_type == 1 ){
		/* if the incoming packet is an IHAVE packet */
		/* here one qustions are what if mutiple nodes declare he has the requested chunk */
		data_packet_list_t *ret = NULL;
		int count = packet->data[0];
		int i;
		for(i = 0;i < count; i ++){
		/* for each hash value generate a GET packet */
		/* each GET packet will only contain ONE hash value*/
		/* TODO(cp2): add a data structrue to record which node has this chunk */
			char data[21];
			memset(data, 0 , 21);
			int j;
			for(j = 0;j < 20;j ++ ){
				data[j] = packet->data[4 + 20 * i + j];
			}

            // Update the chunk owner list
            if(add_to_chunk_owner_list(addr, c_list, data) == -1){
                printf("Error: can't add peer to chunk_owner_list\n");
                return NULL;
            }
			data[20] = '\0';

            data_packet_t *packet = init_packet(2, data);
            
            // Send packet if its the first peer having the chunk
            if(c_list->highest_idx == 0){
                struct sockaddr_in* o_addr = get_chunk_owner(data, c_list);
                spiffy_sendto(sock, packet, sizeof(data_packet_t), 0, (struct sockaddr *) &o_addr, sizeof(struct sockaddr));
                continue;
            }

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
			memset(data, 0 , 20);
		}
		return ret;
	}
	else if( packet->header.packet_type == 2 ){
		/* if the incoming packet is an GET packet */
		/* start transmit of the data */
		char hash[21];
		memcpy(hash, packet->data, 20);
		hash[20] = '\0';

		char *data = get_data_from_hash(hash, config);
		/* pass this data to flow control machine */
		printf("Fetch data from file = %s\n", data);
		return NULL;
	}
	else{
		/* TODO(for next checkpoint) : if incoming packet is other packets*/

	}
	return NULL;
}

data_packet_list_t *generate_WHOHAS(char *chunkfile, chunk_owner_list_t* c_list){
	/*
		this function returns a list of WHOHAS packets when user type GET command
	*/
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
  		if( strlen( line ) > 40 ){
  			fclose(fp);
  			return NULL;
  		}
  		hex2binary((char*)line, 40, (uint8_t *)(data + count));
        
        /* Create and initiallize a owner_list for the chunk */
        init_chunk_owner_list(c_list, (data+count));

  		count += 20;
  	
  		memset(line, 0, 40);

  		if( count >= 1000 ){
  			data[count] = '\0';
  			data_packet_t *packet = init_packet(0,  data);
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
  	data_packet_t *packet = init_packet(0,  data);
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
  	fclose(fp);
  	return ret;
}

/*
 * Below is for chunk operation
 */

void init_chunk_owner_list(chunk_owner_list_t* list, char* data){
    if(list == NULL){
        list = (chunk_owner_list_t*)malloc(sizeof(chunk_owner_list_t));
        memcpy(list->chunk_hash, data, 20);
        list->highest_idx = -1;
        list->chosen_node_idx = -1;
        list->next = NULL;
    }
    else{
        chunk_owner_list_t* new_list = (chunk_owner_list_t*)malloc(sizeof(chunk_owner_list_t));
        memcpy(new_list->chunk_hash, data, 20);
        new_list->highest_idx = -1;
        new_list->chosen_node_idx = -1;
        new_list->next = list;
        list = new_list;
    }    
}

chunk_owner_list_t* search_chunk(chunk_owner_list_t* c_list, char* data){
    while(c_list!=NULL){
        if(strncmp(c_list->chunk_hash, data, 20) == 0)
            return c_list;
        c_list = c_list->next;
    }
    // Return NULL if not found
    return NULL;
}

int add_to_chunk_owner_list(struct sockaddr_in *addr, chunk_owner_list_t* c_list, char* data){
    chunk_owner_list_t* dest_node;
    struct sockaddr_in* new_addr;

    // Search for the right chunk
    if((dest_node=search_chunk(c_list, data)) == NULL)
        return -1;
    // add the addr to the list
    dest_node->highest_idx += 1;
    if(dest_node->highest_idx > 127){
        printf("Node list cannot be more than 128\n");
        return -1;
    }
    new_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    dest_node->list[dest_node->highest_idx] = new_addr;
    return 0;
}

struct sockaddr_in* get_chunk_owner(char* data, chunk_owner_list_t* c_list){
    chunk_owner_list_t* dest_node;
    if((dest_node=search_chunk(c_list, data)) == NULL)
        return NULL;
    if(dest_node->highest_idx == -1)
        return NULL;
    dest_node->chosen_node_idx += 1;
    // Check if the chosen_node_idx is larger than highest_idx
    if(dest_node->chosen_node_idx > dest_node->highest_idx)
        dest_node->chosen_node_idx = 0;
    return dest_node->list[dest_node->chosen_node_idx];
}

