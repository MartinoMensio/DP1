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
#include <sys/wait.h>

#define BACKLOG 250
#define MAX_REQUEST_LEN 512
#define MAX_COMMAND_LEN 10
#define MAX_FILENAME_LEN MAX_REQUEST_LEN
#define ERR_MSG "-ERR\r\n"
#define GET_MSG "GET"
#define QUIT_MSG "QUIT"
#define OK_MSG "+OK\r\n"
#define DATA_CHUNK_SIZE 1024*1024

#define MAX_CHILDREN 3

int sigpipe;

void sigpipeHndlr(int);

int main(int argc, char *argv[]) {
  
  int s_listen, s_accepted;
  int port;
  struct sockaddr_in saddr, caddr;
  FILE *fsock_in, *fsock_out;
  uint caddr_len;
  
  char request[MAX_REQUEST_LEN];
  char type[MAX_COMMAND_LEN];
  char filename[MAX_FILENAME_LEN];
  char buffer[DATA_CHUNK_SIZE];
  
  int receive_requests;
  FILE *file;
  uint32_t file_size, last_modification, file_size_n, last_modification_n;
  struct stat sb;
  uint n_read;
  
  int pid, cpid, nchildren;
  
  
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
  
  nchildren = 0;
  
  while(1) {
    
    // check if can accept and generate a new process
    if(nchildren >= MAX_CHILDREN) {
      // collect a zombie
      printf("PARENT - Reached maximum number of children. Waiting for one of them\n");
      wait(NULL);
      nchildren--;
      printf("PARENT - A children has been waited\n");
      continue;
    }
    
    printf("PARENT - Waiting for a client to connect\n");
    caddr_len = sizeof(caddr);
    s_accepted = accept(s_listen, (struct sockaddr *)&caddr, &caddr_len);
    
    if(s_accepted < 0) {
      perror("PARENT - Impossible to accept");
      break;
    }
    printf("PARENT - Accepted a connection from %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    
    cpid = fork();
    if(cpid < 0) {
      perror("Impossible to fork");
      return 1;
    }
    if(cpid) {
      // parent
      
      // one more child is born
      nchildren++;
      
    } else {
      // child
      pid = getpid();
      sigpipe = 0;
      
      fsock_in = fdopen(s_accepted, "r");
      if(fsock_in == NULL) {
        perror("Impossible to do fdopen r");
        close(s_accepted);
        return 1;
      }
      fsock_out = fdopen(s_accepted, "w");
      if(fsock_out == NULL) {
        perror("Impossible to do fdopen w");
        fclose(fsock_in);
        close(s_accepted);
        return 1;
      }
      
      setbuf(fsock_out, 0);
      
      receive_requests = 1;
      while(receive_requests) {
        memset(request, 0, MAX_REQUEST_LEN);
        if(fgets(request, MAX_REQUEST_LEN, fsock_in) == NULL) {
          perror("Impossible to read a line from the socket. Maybe client closed connection");
          receive_requests = 0;
          break;
        }
        
        printf("CHILD %d - Received a request: %s\n", pid, request);
        
        if(sscanf(request, "%s", type) != 1) {
          // maybe empty line?
          printf("CHILD %d - Type of request not found\n", pid);
          fprintf(fsock_out, ERR_MSG);
          continue;
        }
        
        if(strncmp(type, QUIT_MSG, MAX_COMMAND_LEN) == 0) {
          printf("CHILD %d - Received a quit message\n", pid);
          break;
        }
        if(strncmp(type, GET_MSG, MAX_COMMAND_LEN) != 0) {
          printf("CHILD %d - Received an unknown request type\n", pid);
          fprintf(fsock_out, ERR_MSG);
          break;
        }
        
        // from now on, it is a get message
        if(sscanf(request, "%s %s", type, filename) != 2) {
          printf("CHILD %d - Wrong format in GET request\n", pid);
          fprintf(fsock_out, ERR_MSG);
          continue;
        }
        
        printf("CHILD %d - Parsed request: TYPE=%s FILENAME=%s\n", pid, type, filename);
        
        if(stat(filename, &sb)) {
          perror("Impossible to stat requested file");
          fprintf(fsock_out, ERR_MSG);
          continue;
        }
        
        if((sb.st_mode & S_IFMT) != S_IFREG) {
          perror("The requested file is no a regular file");
          fprintf(fsock_out, ERR_MSG);
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
        
        printf("CHILD %d - Sending size and modification time\n", pid);
        
        fprintf(fsock_out, "%s", OK_MSG);
        fwrite(&file_size_n, sizeof(uint32_t), 1, fsock_out);
        fwrite(&last_modification_n, sizeof(uint32_t), 1, fsock_out);
        
        printf("CHILD %d - Sending the file\n", pid);
        
        while((n_read = fread(buffer, sizeof(char), DATA_CHUNK_SIZE, file)) > 0) {
          fwrite(buffer, sizeof(char), n_read, fsock_out);
          if(sigpipe) {
            printf("CHILD %d - Client closed socket\n", pid);
            receive_requests = 0;
            fclose(file);
            break;
          }
        }
        if(!receive_requests) {
          break;
        }
        fclose(file);
        
        printf("CHILD %d - File sent\n", pid);
      }
      
      fclose(fsock_out);
      fclose(fsock_in);
      close(s_accepted);
      
      printf("CHILD %d - Terminating\n", pid);
      return 0;
    }
    
  }
  return 0; // useless
}

void sigpipeHndlr(int signal) {
  printf("PROCESS %d - SIGPIPE captured!\n", getpid());
  sigpipe = 1;
  return;
}
