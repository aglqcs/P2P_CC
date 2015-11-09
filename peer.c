/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "packet.h"
#include "flow_control.h"
#include <time.h>



extern packet_tracker_t* create_timer(packet_tracker_t *p_tracker, data_packet_t *packet, int sock, struct sockaddr_in *from);;
extern int timer_expired(packet_tracker_t *p_tracker);
extern void process_packet_loss(int offset);


void peer_run(bt_config_t *config);
bt_config_t config;
int sock;

packet_tracker_t *p_tracker = NULL;


data_packet_list_t* reverseList(data_packet_list_t* head){
    data_packet_list_t* cursor = NULL;
    data_packet_list_t* next;
    while (head)
    {
        next = head->next;
        head->next = cursor;
        cursor = head;
        head = next;
    }
    return cursor;
}

/* Broadcast WHOHAS request to all node except myself */
void broadcast(data_packet_t *packet, bt_config_t *config){
    bt_peer_t *node;
    short my_id = config->identity;

    node = config->peers;
    while(node!=NULL){
        // Don't send request to itself
        if(node->id == my_id){
            node = node->next;
            continue;
        }   
        // Send request
        spiffy_sendto(sock, packet, sizeof(data_packet_t), 0, (struct sockaddr *) &node->addr, sizeof(struct sockaddr));
        // Iterate to next node
        node = node->next;
    }         
    return;
}


int main(int argc, char **argv) {

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  }
#endif
  
  peer_run(&config);
  return 0;
}


void process_inbound_udp(int sock) {
  printf("\n"); // DEBUG
  #define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[BUFLEN];

  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

 /* printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
	 "Incoming message from %s:%d\n%s\n\n", 
	 inet_ntoa(from.sin_addr),
	 ntohs(from.sin_port),
	 buf);
*/
  /* first generate the incoming packet */
  data_packet_t *packet = build_packet_from_buf(buf);

  /* next parse this packet and build the response packet*/
  data_packet_list_t *response_list = handle_packet(packet, &config, sock, p_tracker);

  /* finally call spiffy_sendto() to send back the packet*/
  if( NULL == response_list ){
    /* can not generate a response or no data avaiable */
    return;
  }
  else{
    data_packet_list_t *head;
    response_list = reverseList(response_list);
    for( head = response_list; head != NULL; head = head->next ){
      data_packet_t *packet = head->packet;
      
      if( packet->header.packet_type == 3 ){
        printf("DEBUG : SEND DATA SEQ = %d\n", packet->header.seq_num);
      }
       if( packet->header.packet_type == 4 ){
        printf("DEBUG : SEND ACK ACK = %d\n", packet->header.ack_num);
      }
     
      /* check timer if already send and within timeout , then dont send */
      packet_tracker_t *head;
      int find = 0;
      for( head = p_tracker ; head != NULL ;head = head->next ){
        if( head->packet->header.seq_num == packet->header.seq_num && head->packet->header.ack_num == packet->header.ack_num){
          if( -1 == timer_expired(head) ){
            find = 1;
          }
        }
      }
      if( find == 0){
           int r = rand();
         if( r % 20 < 1 &&( packet->header.packet_type == 3 || packet->header.packet_type == 4)){
           fprintf(stderr,"RANDOM DISCARD THIS PACKET\n");
         }
         else{
          int p = spiffy_sendto(sock, packet, sizeof(data_packet_t), 0, (struct sockaddr *) &from, sizeof(struct sockaddr));
            printf("NOMRAL SEND TO %d %d %d\n", p, sock, &from);
    
           printf("ADDR DETAIL = %d, %d\n", from.sin_addr.s_addr, from.sin_port);

          }
          if( packet->header.packet_type == 3){
            printf("=======================\n");
            p_tracker = create_timer(p_tracker, packet, sock, &from);
            printf("=======================\n");

            packet_tracker_t *p = p_tracker;
            printf("------------------AFTER CREATE TIMER----------------\n");
            while( p!= NULL){
                printf("%d - ",p->packet->header.seq_num);
                p = p->next;

            }
            printf("\n------------------AFTER CREATE TIMER----------------\n");

          }

      }
    }
  }
}

void process_get(char *chunkfile, char *outputfile) {
  /* Here open the chunkfile, and write data based on the content of chunkfile*/
  /* notice that here I hard code the length of data to 100, is this enough ?*/ 

  data_packet_list_t *whohas_list = generate_WHOHAS(chunkfile, &config);

  if( NULL == whohas_list){
    printf("can not generate a packet\n");
    return;
  }
  whohas_list = reverseList(whohas_list);
  data_packet_list_t *head;
  for( head = whohas_list; head != NULL; head = head->next){
    data_packet_t *packet = head->packet;
    broadcast(packet, &config);
    /* TODO: call spiffy_sendto() to flood this WHOHAS packet*/
  }
}

void handle_user_input(char *line, void *cbdata) {
  char chunkf[128], outf[128];

  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

  if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
    if (strlen(outf) > 0) {
      process_get(chunkf, outf);
    }
  }
}


void peer_run(bt_config_t *config) {
  struct sockaddr_in myaddr;
  fd_set readfds;
  struct user_iobuf *userbuf;
  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;

  if ((userbuf = create_userbuf()) == NULL) {
    perror("peer_run could not allocate userbuf");
    exit(-1);
  }
  
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
    perror("peer_run could not create socket");
    exit(-1);
  }
  
  bzero(&myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(config->myport);
  
  if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
    perror("peer_run could not bind socket");
    exit(-1);
  }
  
  spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  
  while (1) {
    int nfds;
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    
    nfds = select(sock+1, &readfds, NULL, NULL, &tv);
    
    if (nfds > 0) {
      if (FD_ISSET(sock, &readfds)) {
	process_inbound_udp(sock);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
	process_user_input(STDIN_FILENO, userbuf, handle_user_input,
			   "Currently unused");
      }
    }

    /* loop all the data packet send and check if packet timeout*/
    packet_tracker_t *head;
    for( head = p_tracker ; head != NULL ;head = head->next ){
      if( -1 == timer_expired(head) ){
        /* retransmit*/
        data_packet_t *packet = head->packet;
        /* ignore dummy head */
        if( packet->header.seq_num == -441) 
          continue; 

        printf("DEBUG timer out seq = %d\n", packet->header.seq_num);
        int p = spiffy_sendto(head->sock, packet, sizeof(data_packet_t), 0, (struct sockaddr *)head->from, sizeof(struct sockaddr));
        printf("ERROR SEND TO %d %d %d", p, sock, head->from);
        printf("ADDR DETAIL = %d, %d\n", head->from->sin_addr.s_addr, head->from->sin_port);

        /* reduce the ssthresh */
        process_packet_loss( head->packet->header.ack_num );
        head->send_time = time(NULL);
      }
    }
  }
}



