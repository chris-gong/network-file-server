#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#include "libnetfiles.h"
#include "misc.h"


/*
int main(){
  int nfd1 = netopen("chris.txt", O_RDONLY);
  return 0;
}
*/

int main(int argc, char **argv){
  /*int a;
  for(a = 0; a < 10; a++){
    availablePorts[a].in_use = 0;
    availablePorts[a].port = a + 10001;
  }*/
  /*int clientclientfd;
  char buffer[10000];
  char receivedMessage[10000];
  char sentMessage[10000];*/
  char buffer[10000];
  /*if(argc != 3){
    printf("Too few/many arguments\n");
    return 0;
  }*/
  /*globalhostname = argv[1];
  globalport = argv[2];*/
  char filemode_[100];
  printf("Enter a file mode: Unrestricted = 0, Exclusive = 1, Transaction = 2\n");
  scanf("%s", filemode_);
  int filemode = atoi(filemode_);
  netserverinit("localhost", filemode);
  /*clientfd = get_clientfd(argv[1], argv[2]);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return 0;
  }
  */
  while(fgets(buffer, 10000, stdin) != NULL){
    /*clientfd = get_clientfd(argv[1], argv[2]);
    if(clientfd < 0){
      printf("Failed to connect to server, shutting down program\n");
      return 0;
    }*/
    char input[5000];
    int len = strlen(buffer);
    memcpy(input, buffer, len);
    input[len - 1] = '\0';
    if(strcmp(input, "open") == 0){
      char pathname[5000];
      char flags_[5000];
      int flags = -1;
      printf("Enter the pathname\n");
      scanf("%s", pathname);
      
      printf("Enter flags\n");
      scanf("%s", flags_); //scanf appends null terminating character to the end of input
     
     if(strcmp(flags_,"O_RDONLY") == 0){
        flags = O_RDONLY;
      }
      else if(strcmp(flags_,"O_WRONLY") == 0){
        flags = O_WRONLY;
      }
      else if(strcmp(flags_,"O_RDWR") == 0){
        flags = O_RDWR;
      } 
      netopen(pathname, flags);
    }
    else if(strcmp(input, "read") == 0){
      char buf[5000]; //netread on a buffer that's too small
      char nfd[5000];
      char nBytes[5000];
      printf("Enter network file descriptor\n");
      scanf("%s", nfd);
      printf("Enter number of bytes to be read in\n");
      scanf("%s", nBytes);
      int networkfd = atoi(nfd);
      size_t bytes = (size_t) atoi(nBytes);
      int result = netread(networkfd, buf, bytes);
      buf[result] = '\0';
      printf("Read result: %d\n", result);
      printf("Buffer contains: %s\n", buf);


    }
    else if (strcmp(input, "write") == 0) {
      char buf[5000];
      char nfd[5000];
      int networkfd = 0;
      char numBytes[100];
      printf("Enter network file descriptor\n");
      fgets(nfd, 5000, stdin);
      printf("Enter text to write\n");
      fgets(buf, 5000, stdin);
      char *last = &buf[strlen(buf)-1];
        *last = '\0';
      //printf("Buffer: %s\n", buf);
      printf("Enter number of bytes to write\n");
      scanf("%s", numBytes);

      networkfd = atoi(nfd);
      size_t bytes = (size_t) atoi(numBytes);
      netwrite(networkfd, buf, bytes);
    }
    else if (strcmp(input, "close") == 0) {
      int nfd = 0;
      printf("Enter network file descriptor\n");
      scanf("%d", &nfd);
      netclose(nfd);

    }

    /*write(clientfd, sentMessage, sizeof(sentMessage));
    read(clientfd, receivedMessage, sizeof(receivedMessage));
    printf("%s\n", receivedMessage);
    memset(buffer, 0, sizeof(buffer));
    memset(receivedMessage, 0, sizeof(receivedMessage));
    memset(sentMessage, 0, sizeof(sentMessage));*/
    memset(buffer, 0, sizeof(buffer));
  }
  return 0;
}
