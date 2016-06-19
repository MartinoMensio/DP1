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
  char file_name[MAX_FILENAME_LEN];
  FILE *fsock_in, *fsock_out;
  char command[MAX_COMMAND_LEN];
  uint32_t file_size, last_modification;
  char *buffer;
  
  FILE *file;
  
  message request, response;
  XDR xdrs_in, xdrs_out;
    
  if(argc != 3) {
    fprintf(stderr, "Usage: %s <address> <port>\n", argv[0]);
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
  
  printf("Ready. Use CTRL-D to quit\n");
  
  while(1) {
    memset(&request, 0, sizeof(message));
    memset(&response, 0, sizeof(message));
    printf("> ");
    if(fgets(line, MAX_FILENAME_LEN, stdin) == NULL) {
      printf("You decided to quit\n");
      //fprintf(fsock_out,"%s\r\n", QUIT_MSG);
      request.tag = QUIT;
      xdr_message(&xdrs_out, &request);
      break;
    }
    if(sscanf(line, "%s", file_name) != 1) {
      printf("Please provide a file name!\n");
      continue;
    }
    
    printf("you asked for file: %s\n", file_name);
    //fprintf(fsock_out, "%s %s\r\n", GET_MSG, file_name);
    request.tag = GET;
    //strcpy(request.message_u.filename, file_name);
    request.message_u.filename =  file_name;
    xdr_message(&xdrs_out, &request);
    if(sigpipe) {
      printf("Socket was closed by server\n");
      break;
    }
    
    // set pointer to null, so that xdr will allocate it
    //response.message_u.fdata.contents.contents_val = NULL;
    //fgets(command, MAX_COMMAND_LEN, fsock_in);
    xdr_message(&xdrs_in, &response);
    //if(strncmp(command, ERR_MSG, MAX_COMMAND_LEN) == 0) {
    if(response.tag == ERR) {
      printf("Received an error message from server\n");
      continue;
    }
    
    //if(strncmp(command, OK_MSG, MAX_COMMAND_LEN) != 0) {
    if(response.tag != OK) {
      printf("Received an unknown response from server: %s\n", command);
      break;
    }
    
    file = fopen(file_name, "w");
    if(file == NULL) {
      perror("Impossible to open for creation requested file");
      continue;
    }
    /*
    if(fread(&file_size_n, sizeof(uint32_t), 1, fsock_in) < 1) {
      printf("Error receiving file size\n");
      break;
    }*/
    
    //file_size = ntohl(file_size_n);
    file_size = response.message_u.fdata.contents.contents_len;
    printf("Received file size: %u\n", file_size);
    /*
    if(fread(&last_modification_n, sizeof(uint32_t), 1, fsock_in) < 1) {
      printf("Error receiving last modification\n");
      break;
    }*/
    //last_modification = ntohl(last_modification_n);
    last_modification = response.message_u.fdata.last_mod_time;
    printf("Received last modification: %u\n", last_modification);
    
    buffer = response.message_u.fdata.contents.contents_val;
    /*
    while((n_read = fread(buffer, sizeof(char), (file_size > DATA_CHUNK_SIZE)? DATA_CHUNK_SIZE : file_size, fsock_in)) > 0) {
      file_size -= n_read;
      fwrite(buffer, sizeof(char), n_read, file);
    }*/
    fwrite(buffer, sizeof(char), file_size, file);
    
    fclose(file);
    

    printf("File received correctly\n");

  }
  
  xdr_destroy(&xdrs_in);
  xdr_destroy(&xdrs_in);
  
  fclose(fsock_in);
  fclose(fsock_out);
  
  close(s);
  return 0;
}

void sigpipeHndlr(int signal) {
  printf("SIGPIPE captured!\n");
  sigpipe = 1;
  return;
}
