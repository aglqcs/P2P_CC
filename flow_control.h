
#define CHUNK_PACKET_NUMBER 512

#define UNSEND 1
#define UNACKED 2
#define ACKED 3

#define SLOWSTART 1
#define CON_AVOID 2
#define FAST_RETRANSMIT 3

typedef struct data{
  char *hash;
  char *content;
  char state[CHUNK_PACKET_NUMBER];
  int send_window;
  int ssthresh;
  char congestion_control_state;
  int start;
  int end;
} data_t;  


typedef struct data_list{
	data_t *data;
	struct data_list *next;
} data_list_t;

