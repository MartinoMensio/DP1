#include <stdlib.h>
#include <stdio.h>
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
#include <unistd.h>
#include <signal.h>
#include <rpc/xdr.h>
#include "../types.h"

#define MAX_REQUEST_LEN 512
#define MAX_COMMAND_LEN 10
#define MAX_FILENAME_LEN MAX_REQUEST_LEN
#define ERR_MSG "-ERR\r\n"
#define GET_MSG "GET"
#define QUIT_MSG "QUIT"
#define OK_MSG "+OK\r\n"
#define DATA_CHUNK_SIZE 1024*1024

int sigpipe;

void sigpipeHndlr(int);

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr;
  int port;
  int s;
  char line[MAX_FILENAME_LEN];
  char *file_name;
  FILE *fsock_in, *fsock_out;
  char command[MAX_COMMAND_LEN];
  uint32_t file_size, file_size_n;
  char buffer[DATA_CHUNK_SIZE];
  
  uint n_read;
  
  FILE *file;
  
  call_msg request;
  response_msg response;
  XDR xdrs_in, xdrs_out;
    
  if(argc != 4) {
    fprintf(stderr, "Usage: %s <address> <port> <filename>\n", argv[0]);
    return 1;
  }
  if(!inet_aton(argv[1], &saddr.sin_addr)) {
    fprintf(stderr, "Invalid address\n");
    return 1;
  }
  port = atoi(argv[2]);
  if(port == 0) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }
  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(s < 0) {
    perror("Impossible to create socket");
    return 1;
  }
  
  printf("Socket created\n");
  
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to connect");
    return 1;
  }
  
  printf("Socket connected\n");
  
  fsock_in = fdopen(s, "r");
  fsock_out = fdopen(s, "w");
  
  if(fsock_in == NULL || fsock_out == NULL) {
    perror("Impossible to fdopen");
    return 1;
  }
  
  // unbuffered write
  setbuf(fsock_out, 0);
  
  xdrstdio_create(&xdrs_in, fsock_in, XDR_DECODE);
  xdrstdio_create(&xdrs_out, fsock_out, XDR_ENCODE);
  
  // set SIGPIPE handler in order to avoid crashes
  sigpipe = 0;
  signal(SIGPIPE, sigpipeHndlr);
  

  memset(&request, 0, sizeof(call_msg));
  memset(&response, 0, sizeof(response_msg));
  
  file_name = argv[3];
  
  printf("you asked for file: %s\n", file_name);

  request.ctype = GET;

  request.call_msg_u.filename =  file_name;
  xdr_call_msg(&xdrs_out, &request);
  if(sigpipe) {
    printf("Socket was closed by server\n");
    return 1;
  }
  
  // set pointer to null, so that xdr will allocate it
  //response.message_u.fdata.contents.contents_val = NULL;
  //fgets(command, MAX_COMMAND_LEN, fsock_in);
  xdr_response_msg(&xdrs_in, &response);
  //if(strncmp(command, ERR_MSG, MAX_COMMAND_LEN) == 0) {
  if(response == ERR) {
    printf("Received an error message from server\n");
    return 1;
  }
  
  if(response != OK) {
    printf("Received an unknown response from server: %s\n", command);
    return 1;
  }
  
  file = fopen(file_name, "w");
  if(file == NULL) {
    perror("Impossible to open for creation requested file");
    return 1;
  }
  
  if(fread(&file_size_n, sizeof(uint32_t), 1, fsock_in) < 1) {
    printf("Error receiving file size\n");
    return 1;
  }
  
  file_size = ntohl(file_size_n);
  printf("Received file size: %u\n", file_size);

  
  //buffer = response.message_u.fdata.contents.contents_val;
  
  while((n_read = fread(buffer, sizeof(char), (file_size > DATA_CHUNK_SIZE)? DATA_CHUNK_SIZE : file_size, fsock_in)) > 0) {
    file_size -= n_read;
    fwrite(buffer, sizeof(char), n_read, file);
  }
  
  
  fclose(file);
  

  printf("File received correctly\n");

  request.ctype = QUIT;
  xdr_call_msg(&xdrs_out, &request);
  
  xdr_destroy(&xdrs_in);
  xdr_destroy(&xdrs_in);
  
  fclose(fsock_in);
  fclose(fsock_out);
  
  close(s);
  return 3;
}

void sigpipeHndlr(int signal) {
  printf("SIGPIPE captured!\n");
  sigpipe = 1;
  return;
}
