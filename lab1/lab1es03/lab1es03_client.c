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

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr;
  int port;
  int s;
  char line[MAX_LINE_LEN];
  int a, b;
  FILE *fsock_in, *fsock_out;
  
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
    fprintf(stderr, "Impossible to create socket\n");
    return 1;
  }
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    fprintf(stderr, "Impossible to connect: \n");
    perror(NULL);
    return 1;
  }
  printf("Connection estabilished. Write 'end' to quit\n");
  
  fsock_in = fdopen(s, "r");
  fsock_out = fdopen(s, "w");
  
  if(fsock_in == NULL || fsock_out == NULL) {
    fprintf(stderr, "Impossible to fdopen\n");
    return 1;
  }
  
  // unbuffered write
  setbuf(fsock_out, 0);
  
  while(1) {
    fgets(line, MAX_LINE_LEN, stdin);
    if(strncmp(line, "end\n", MAX_LINE_LEN) == 0) {
      break;
    }
    if(sscanf(line, "%d %d", &a, &b) != 2) {
      printf("Wrong input\n");
    } else {
      fprintf(fsock_out, "%d %d\r\n", a, b);
      printf("Sent\n");
      fgets(line, MAX_LINE_LEN, fsock_in);
      printf("Received answer: %s\n", line);
    }
  }
  
  close(s);
  return 0;
}

