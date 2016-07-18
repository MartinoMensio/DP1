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

char *prog_name;

#define MEGA 1024*1024
#define MAX_FILENAME 512

int sigpipe;
void sigpipeHndlr(int);

int main (int argc, char *argv[])
{
  struct sockaddr_in saddr;
  int port;
  int s;
  
  FILE *fsock_in, *fsock_out;
  char buffer[MEGA];
  
  char *file_name;
  FILE *file;
  uint n_read;
  
  int send_complete;
  int n_mega;
  uint32_t hc, hc_n;
  
  if(argc != 5) {
    fprintf(stderr, "Usage: %s <address> <port> <n_mega> <file_name>\n", argv[0]);
    return 2;
  }
  if(!inet_aton(argv[1], &saddr.sin_addr)) {
    fprintf(stderr, "Invalid address\n");
    return 2;
  }
  port = atoi(argv[2]);
  if(port == 0) {
    fprintf(stderr, "Invalid port\n");
    return 2;
  }
  n_mega = atoi(argv[3]);
  if(n_mega == 0) {
    fprintf(stderr, "Invalid n_mega\n");
    return 2;
  }
  file_name = argv[4];
  file = fopen(file_name, "r");
  if(file == NULL) {
    fprintf(stderr, "Impossible to open requested file %s\n", file_name);
    return 1;
  }
  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(s < 0) {
    perror("Impossible to create socket");
    return 2;
  }
  
  printf("Socket created\n");
  
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to connect");
    return 2;
  }
  
  printf("Socket connected\n");
  
  fsock_in = fdopen(s, "r");
  fsock_out = fdopen(s, "w");
  
  if(fsock_in == NULL || fsock_out == NULL) {
    fprintf(stderr, "Impossible to fdopen\n");
    return 2;
  }
  
  // unbuffered write
  setbuf(fsock_out, 0);
  
  // set SIGPIPE handler in order to avoid crashes on write
  sigpipe = 0;
  signal(SIGPIPE, sigpipeHndlr);
  
  send_complete = 1;
  
  n_read = fread(buffer, sizeof(char), MEGA, file);
  if(n_read != MEGA) {
    fprintf(stderr, "The file size is not at least 1 MB\n");
    return 1;
  }
  fclose(file);
  
  for(int i = 0; i < n_mega; i++) {
    fwrite(buffer, sizeof(char), n_read, fsock_out);
    if(sigpipe) {
      fprintf(stderr, "Impossible to write because of SIGPIPE\n");
      send_complete = 0;
      break;
    }
  }
  
  if(send_complete) {
    fread(&hc_n, sizeof(uint32_t), 1, fsock_in);
    hc = ntohl(hc_n);
    fprintf(stdout, "%d", hc);
  }
    
  
  fclose(fsock_in);
  fclose(fsock_out);
  
  return 0;
}

void sigpipeHndlr(int signal) {
  printf("SIGPIPE captured!\n");
  sigpipe = 1;
  return;
}
