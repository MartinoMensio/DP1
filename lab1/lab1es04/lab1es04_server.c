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
  
  struct sockaddr_in saddr, caddr;
  int port;
  int s;
  char line[MAX_LINE_LEN + 1];
  int nReceived;
  uint caddrLen;
  
  if(argc != 2) {
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
  
  s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(s < 0) {
    perror("Impossible to create socket");
    return 1;
  }
  
  if(bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    perror("Impossible to bind");
    return 1;
  }
  
  while(1) {
    memset(line, 0, MAX_LINE_LEN + 1);
    caddrLen = sizeof(caddr);
    nReceived = recvfrom(s, line, MAX_LINE_LEN, 0, (struct sockaddr *)&caddr, &caddrLen);
    printf("Received from %s: %s\n", inet_ntoa(caddr.sin_addr), line);
    sendto(s, line, nReceived, 0, (struct sockaddr *)&caddr, caddrLen);
    printf("Sent response\n");
  }
  
  close(s);
  return 0;
}
