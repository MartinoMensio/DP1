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
#define MAX_REQUESTS 3
#define HISTORY_LENGTH 10
#define MAX_ENTRY_LENGTH 20

typedef struct {
  char entries[HISTORY_LENGTH][MAX_ENTRY_LENGTH];
  int requests[HISTORY_LENGTH];
  int current;
} history;

history historyCreate();
int historyCheck(history *h, char *key);

int main(int argc, char *argv[]) {
  
  struct sockaddr_in saddr, caddr;
  int port;
  int s;
  char line[MAX_LINE_LEN + 1];
  int nReceived;
  uint caddrLen;
  history h;
  
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
  h = historyCreate();
  
  while(1) {
    memset(line, 0, MAX_LINE_LEN + 1);
    caddrLen = sizeof(caddr);
    nReceived = recvfrom(s, line, MAX_LINE_LEN, 0, (struct sockaddr *)&caddr, &caddrLen);
    //sleep(4);
    printf("Received from %s: %s\n", inet_ntoa(caddr.sin_addr), line);
    if(historyCheck(&h, inet_ntoa(caddr.sin_addr)) >= MAX_REQUESTS) {
      printf("banned\n");
      continue;
    }
    
    sendto(s, line, nReceived, 0, (struct sockaddr *)&caddr, caddrLen);
    printf("Sent response\n");
  }
  
  close(s);
  return 0;
}

history historyCreate() {
  int i;
  history h;
  h.current = 0;
  for(i = 0; i < HISTORY_LENGTH; i++) {
    h.requests[i] = 0;
    memset(h.entries[i], 0, MAX_ENTRY_LENGTH);
  }
  return h;
}
// returns the count (before update)
int historyCheck(history *h, char* key) {
  int i, found, times = 0;
  for(i = 0, found = -1; i < HISTORY_LENGTH && found < 0; i++) {
    if(strncmp(h->entries[i], key, MAX_ENTRY_LENGTH) == 0) {
      found = i;
      times = h->requests[i]++;
    }
  }
  if(found < 0) {
    strncpy(h->entries[h->current], key, MAX_ENTRY_LENGTH);
    h->requests[h->current] = 1;
    h->current = (h->current + 1) % HISTORY_LENGTH;
  }
  return times;
}
