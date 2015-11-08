#include <sys/types.h>
#include "bt_parse.h"
#include <sys/socket.h>


#define PACKETLEN 1500

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
  char data[1200];
} data_packet_t;

typedef struct data_packet_list{
	data_packet_t *packet;
	struct data_packet_list *next;
} data_packet_list_t;


typedef struct packet_tracker{
    int sock;
    struct sockaddr_in *from;
    time_t send_time;
    int expire_time;
    data_packet_t *packet;
    struct packet_tracker *next;
} packet_tracker_t;

data_packet_t *init_packet(char type, char *data, int length);
int read_chunkfile(char * chunkfile, char *ret);
data_packet_t *build_packet_from_buf(char *buf);
data_packet_list_t *handle_packet(data_packet_t *packet, bt_config_t* config, int sockfd, packet_tracker_t *p_tracker);
data_packet_list_t *generate_WHOHAS(char *chunkfile, bt_config_t* config);