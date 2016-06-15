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

#define MAX_LINE_LEN 100
#define TIMEOUT 3
#define MAX_TIMES 5

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr;
  int port;
  int s;
  char line[MAX_LINE_LEN + 1];
  char line_tmp[MAX_LINE_LEN + 1];
  FILE *fsock_in, *fsock_out;
  int result;
  fd_set fds;
  struct timeval tv;
  int i;
  
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
  
  s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(s < 0) {
    fprintf(stderr, "Impossible to create socket\n");
    return 1;
  }
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    fprintf(stderr, "Impossible to connect: \n");
    perror(NULL);
    return 1;
  }
  
  fsock_in = fdopen(s, "r");
  fsock_out = fdopen(s, "w");
  
  if(fsock_in == NULL || fsock_out == NULL) {
    fprintf(stderr, "Impossible to fdopen\n");
    return 1;
  }
  
  // unbuffered write
  setbuf(fsock_out, 0);
  
  printf("Ready. Write 'end' to quit\n");
  
  while(1) {
    fgets(line, MAX_LINE_LEN, stdin);
    if(strncmp(line, "end\n", MAX_LINE_LEN) == 0) {
      break;
    }
    for(i = 0; i < MAX_TIMES; i++) {
      
      // check if more data is still on the socket to be read
      while(1) {
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        result = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
        if(result < 0) {
          fprintf(stderr, "Error in select: ");
          perror(NULL);
          return 1;
        }
        if(result == 0) {
          break;
        }
        read(s, line_tmp, MAX_LINE_LEN);
      }
      
      
      fprintf(fsock_out, "%s", line);
      printf("Sent\n");
      
      FD_ZERO(&fds);
      FD_SET(s, &fds);
      tv.tv_sec = TIMEOUT;
      tv.tv_usec = 0;
      result = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
      if(result < 0) {
        fprintf(stderr, "Error in select: ");
        perror(NULL);
        return 1;
      }
      if(result == 0) {
        // timed out
        printf("Timed out. Retrying\n");
      } else {
        break;
      }
    }
    if(i == MAX_TIMES) {
      printf("Maximum number of retransmission reached. Suicide.\n");
      return 1;
    }
    //if(read(s, line, MAX_LINE_LEN) <= 0) {
    if(fgets(line, MAX_LINE_LEN, fsock_in) == NULL) {
      printf("Impossible to receive answer\n");
      return 1;
    }
    
    
    
    printf("Received answer: %s\n", line);
  }
  
  close(s);
  return 0;
}
