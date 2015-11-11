#include "log.h"
#include <time.h>
#include <netinet/in.h>

#define CHUNK_PACKET_NUMBER 512

#define UNSEND 1
#define UNACKED 2
#define ACKED 3

#define SLOWSTART 1
#define CON_AVOID 2
#define FAST_RETRANSMIT 3

#define TRUE 1
#define FALSE 0

#define TIMEOUT 1

typedef struct data{
	int socket;
	int offset;
	char *content; // 512 * 1024 byte data
	char state[CHUNK_PACKET_NUMBER];
	int send_window;
 	int ssthresh;
  	char congestion_control_state;
  	int start;
  	int end;
  	float increase_rate;
  	u_int prev_ack;
  	int count_ack;
} data_t;  


typedef struct data_list{
	data_t *data;
	struct data_list *next;
} data_list_t;



typedef struct data_chunk{
	int acked;
	int recved;
	int length;
	char content[1500];
} data_chunk_t;


typedef struct recv_buffer{
	int expected;
	int socket;
	int offset;
	int busy;
	int total_recv;
	data_chunk_t chunks[CHUNK_PACKET_NUMBER];
} recv_buffer_t;

typedef struct recv_buffer_list{
	recv_buffer_t *buffer;
	struct recv_buffer_list *next;
} recv_buffer_list_t;

typedef struct chunk_owner_list{
    // Chunk hash map
  int offset;
  struct sockaddr_in *address;
  struct chunk_owner_list *next;
} chunk_owner_list_t;

typedef struct File_manager{
  int init;
  int chunk_count;
  int *offset;
  int top;
  time_t *timer;
  char *output_location;
  chunk_owner_list_t *already_used;
} file_manager_t;






