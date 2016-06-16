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

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr;
  int port;
  int s;
  
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
  if(connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to connect");
    return 1;
  }
  printf("Connection estabilished\n");
  
  close(s);
  return 0;
}
