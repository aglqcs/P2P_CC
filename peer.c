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

void peer_run(bt_config_t *config);
void broadcast(data_packet_t *packet, bt_config_t *config);
bt_config_t config;
int sock;

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
  #define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[BUFLEN];
  int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

  printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
	 "Incoming message from %s:%d\n%s\n\n", 
	 inet_ntoa(from.sin_addr),
	 ntohs(from.sin_port),
	 buf);

  printf("packet not build yet\n");
  /* first generate the incoming packet */
  data_packet_t *packet = build_packet_from_buf(buf);
 
  printf("packet build: %s\n", packet->data); 
  if(packet->header.packet_type == '0'){
    printf("Receive WHOHAS\n");
  }
  else if(packet->header.packet_type == '1'){
    printf("Receive IHAVE\n");
  }
  else{
    printf("other requets: %c\n", packet->header.packet_type);
  }
  /* next parse this packet and build the response packet*/
  data_packet_t *response = handle_packet(packet, &config);

  /* finally call spiffy_sendto() to send back the packet*/
  /* TODO: send back the response*/
  if(response != NULL && response->header.packet_type == '1'){
    printf("Send IHAVE\n");
    spiffy_sendto(sock, response, sizeof(data_packet_t), 0, (struct sockaddr *) &from, sizeof(struct sockaddr));
  }
}

void process_get(char *chunkfile, char *outputfile) {
  /* Here open the chunkfile, and write data based on the content of chunkfile*/
  /* notice that here I hard code the length of data to 100, is this enough ?*/ 
  char data[100];
  int count;
  if( (count = read_chunkfile(chunkfile, data)) < 0){
    return;
  }
  /* build the WHOHAS packet   WHOHAS = '0'*/
  data_packet_t *packet = init_packet('0',  data);

  /* call spiffy_sendto() to flood the WHOHAS packet */
  broadcast(packet, &config);
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
  // int sock;
  struct sockaddr_in myaddr;
  fd_set readfds;
  struct user_iobuf *userbuf;
  
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
    
    nfds = select(sock+1, &readfds, NULL, NULL, NULL);
    
    if (nfds > 0) {
      if (FD_ISSET(sock, &readfds)) {
          printf("FD_ISSET socket from readfd\n");
	process_inbound_udp(sock);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
	process_user_input(STDIN_FILENO, userbuf, handle_user_input,
			   "Currently unused");
      }
    }
  }
}

/* Broadcast WHOHAS request to all node except myself */
void broadcast(data_packet_t *packet, bt_config_t *config){
    bt_peer_t *node;
    short my_id = config->identity;
    // int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

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

/* Receive IHAVE */
