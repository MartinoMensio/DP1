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

#define MAX_LINE_LEN 100

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr;
  int port;
  int s;
  char line[MAX_LINE_LEN];
  int a, b;
  FILE *fsock_in, *fsock_out;
  
  // check arguments
  if(argc != 3) {
    fprintf(stderr, "Usage: %s <address> <port>\n", argv[0]);
    return 1;
  }
  // parse the address
  if(!inet_aton(argv[1], &saddr.sin_addr)) {
    fprintf(stderr, "Invalid address\n");
    return 1;
  }
  // parse the port
  port = atoi(argv[2]);
  if(port == 0) {
    fprintf(stderr, "Invalid port\n");
    return 1;
  }
  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  
  // create the socket TCP
  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(s < 0) {
    perror("Impossible to create socket");
    return 1;
  }
  // connect the socket
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to connect");
    return 1;
  }
  printf("Connection estabilished. Write 'end' to quit\n");
  
  // create FILE* variables to work on socket by using stdio
  fsock_in = fdopen(s, "r");
  fsock_out = fdopen(s, "w");
  
  if(fsock_in == NULL || fsock_out == NULL) {
    fprintf(stderr, "Impossible to fdopen\n");
    return 1;
  }
  
  // unbuffered write, because stdio must work directly without buffering
  setbuf(fsock_out, 0);
  
  // main loop
  while(1) {
    // get a line from keyboard
    fgets(line, MAX_LINE_LEN, stdin);
    // termination test: "end" makes the program terminate
    if(strncmp(line, "end\n", MAX_LINE_LEN) == 0) {
      break;
    }
    // check the format
    if(sscanf(line, "%d %d", &a, &b) != 2) {
      printf("Wrong input\n");
      // then loop repeats asking again for a line
    } else {
      // printf on the socket
      fprintf(fsock_out, "%d %d\r\n", a, b);
      printf("Sent\n");
      // read from the socket a line
      fgets(line, MAX_LINE_LEN, fsock_in);
      printf("Received answer: %s\n", line);
    }
  }
  
  // close all
  fclose(fsock_in);
  fclose(fsock_out);
  
  close(s);
  return 0;
}

