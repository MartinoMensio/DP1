/*
 *  TEMPLATE
 */

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

#define BACKLOG 250
#define MAX_REQUEST_LEN 512
#define MAX_COMMAND_LEN 10
#define MAX_FILENAME_LEN MAX_REQUEST_LEN
#define ERR_MSG "-ERR\r\n"
#define GET_MSG "GET"
#define QUIT_MSG "QUIT"
#define OK_MSG "+OK\r\n"
#define DATA_CHUNK_SIZE 1024*1024

int sigpipe;
char *prog_name;

void sigpipeHndlr(int);

int pids[3];

void killMyChildren(int);

int main(int argc, char *argv[]) {
  
  int s_listen, s_accepted;
  int port;
  struct sockaddr_in saddr, caddr;
  FILE *fsock_in, *fsock_out;
  uint caddr_len;
  
  char request_str[MAX_REQUEST_LEN];
  char type[MAX_COMMAND_LEN];
  char filename[MAX_FILENAME_LEN];
  char buffer[DATA_CHUNK_SIZE];
  char *buffer2;    // for xdr
  
  int receive_requests;
  FILE *file;
  uint32_t file_size, last_modification, file_size_n, last_modification_n;
  struct stat sb;
  uint n_read;
  
  call_msg request;
  response_msg response;
  XDR xdrs_in, xdrs_out;
  
  int pid;
  
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    return 1;
  }

  port = atoi(argv[1]);
  if(port == 0) {
    fprintf(stderr, "Invalid port\n");
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
  //signal(SIGINT, killMyChildren);
  
  for(int i = 0; i < 3; i++) {
    pid = fork();
    if(pids[i] < 0) {
      printf("Impossible to create another process\n");
      break;
    }
    if(pid) {
      // parent
      pids[i] = pid;
    } else {
      // child
      break;
    }
  }
  
  if(pid) {
    killMyChildren(0);
    return 0;
  }
  
  
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
    
    xdrstdio_create(&xdrs_in, fsock_in, XDR_DECODE);
    xdrstdio_create(&xdrs_out, fsock_out, XDR_ENCODE);
    
    setbuf(fsock_out, 0);
    setbuf(fsock_in, 0);
    
    receive_requests = 1;
    while(receive_requests) {
      memset(request_str, 0, MAX_REQUEST_LEN);
      memset(&request, 0, sizeof(call_msg));
      memset(&response, 0, sizeof(response_msg));
      
      if(!xdr_call_msg(&xdrs_in, &request)) {
        printf("Error receiving request. Client closed connection\n");
        break;
      }
      if(request.ctype == QUIT) {
        printf("Client asked to quit\n");
        break;
      } else if(request.ctype != GET) {
        printf("Unknown request received\n");
        break;
      }
      strncpy(filename, request.call_msg_u.filename, FILENAME_MAX);
      printf("Received a GET request for file: %s\n", filename);
        
      
      if(stat(filename, &sb)) {
        perror("Impossible to stat requested file");
        response = ERR;
        xdr_response_msg(&xdrs_out, &response);
        continue;
      }
      
      if((sb.st_mode & S_IFMT) != S_IFREG) {
        perror("The requested file is no a regular file");
        response = ERR;
        xdr_response_msg(&xdrs_out, &response);
        continue;
      }
      
      file_size = sb.st_size;
      last_modification = sb.st_mtim.tv_sec;
      file_size_n = htonl(file_size);
      last_modification_n = htonl(last_modification);
      
      file = fopen(filename, "r");
      if(file == NULL) {
        perror("Impossible to open requested file");
        fprintf(fsock_out, ERR_MSG);
        continue;
      }

      printf("Sending message\n");
      response = OK;
      xdr_response_msg(&xdrs_out, &response);

      printf("Sending size and modification time\n");
      
      fwrite(&file_size_n, sizeof(uint32_t), 1, fsock_out);
      //fwrite(&last_modification_n, sizeof(uint32_t), 1, fsock_out);
      
      printf("Sending the file\n");
      
      while((n_read = fread(buffer, sizeof(char), DATA_CHUNK_SIZE, file)) > 0) {
        fwrite(buffer, sizeof(char), n_read, fsock_out);
        if(sigpipe) {
          printf("Client closed socket\n");
          receive_requests = 0;
          fclose(file);
          break;
        }
      }
      if(!receive_requests) {
        break;
      }
      fclose(file);
      
      printf("File sent\n");
    }
    
    xdr_destroy(&xdrs_in);
    xdr_destroy(&xdrs_out);
    
    fclose(fsock_out);
    fclose(fsock_in);
    close(s_accepted);
    
  }
  return 0;
}

void sigpipeHndlr(int signal) {
  printf("SIGPIPE captured!\n");
  sigpipe = 1;
  return;
}

void killMyChildren(int p) {
  for(int i = 0; i < 3; i++) {
    kill(pids[i], SIGKILL);
  }
}
