/*
 *  TEMPLATE
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "../utils.h"

#define MEGA 1024*1024
#define BACKLOG 200

char *prog_name;

void sigpipeHndlr(int);
void sigIntHndlr(int);

char *buffer;
int sigpipe;

int main (int argc, char *argv[])
{
  struct sockaddr_in saddr, caddr;
  uint caddr_len;
  int port;
  int s_listen, s_accepted;
  FILE *fsock_in, *fsock_out;
  
  int n_mega;
  uint32_t hc, hc_n;
  
  
  
  
  if(argc != 3) {
    fprintf(stderr, "Usage: %s <port> <n_mega>\n", argv[0]);
    return 1;
  }
  port = atoi(argv[1]);
  if(port == 0) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }
  n_mega = atoi(argv[2]);
  if(n_mega == 0) {
    fprintf(stderr, "Invalid n_mega\n");
    return 1;
  }
  buffer = calloc(n_mega, MEGA);
  if(buffer == NULL) {
    fprintf(stderr, "Error allocating buffer of %d MB\n", n_mega);
    return 1;
  }
  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  
  s_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(s_listen < 0) {
    perror("Impossible to create socket");
    return 1;
  }
  
  printf("Created listen socket\n");
  
  if(bind(s_listen, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to bind");
    return 1;
  }
  
  printf("Socket bound to address\n");
  
  if(listen(s_listen, BACKLOG) < 0) {
    perror("Impossible to listen");
    return 1;
  }
  
  printf("Socket listen done\n");
  
  signal(SIGPIPE, sigpipeHndlr);
  signal(SIGINT, sigIntHndlr);
  
  hc = 1;
  
  while(1) {
    caddr_len = sizeof(caddr);
    
    printf("Waiting for a client to connect\n");
    
    s_accepted = accept(s_listen, (struct sockaddr *)&caddr, &caddr_len);
    
    sigpipe = 0;
    
    if(s_accepted < 0) {
      perror("Impossible to accept");
      break;
    }
    
    printf("Accepted a connection from %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    
    fsock_in = fdopen(s_accepted, "r");
    if(fsock_in == NULL) {
      perror("Impossible to do fdopen r");
      close(s_accepted);
      continue;
    }
    fsock_out = fdopen(s_accepted, "w");
    if(fsock_out == NULL) {
      perror("Impossible to do fdopen w");
      fclose(fsock_in);
      close(s_accepted);
      continue;
    }
    
    setbuf(fsock_out, 0);
    
    if(fread(buffer, MEGA, n_mega, fsock_in) != n_mega) {
      fprintf(stderr, "Impossible to receive the whole size of data\n");
    } else {
      hc = hashCode(buffer, n_mega * MEGA, hc);
      fprintf(stdout, "%d", hc);
      hc_n = htonl(hc);
      fwrite(&hc_n, sizeof(uint32_t), 1, fsock_out);
    }
    
    
    fclose(fsock_in);
    fclose(fsock_out);
    close(s_accepted);
  }
  
  close(s_listen);

  return 0;
}

void sigpipeHndlr(int signal) {
  printf("SIGPIPE captured!\n");
  sigpipe = 1;
  return;
}

void sigIntHndlr(int signal){
  printf("SIGINT captured!\n");
  free(buffer);
  exit(0);
}
