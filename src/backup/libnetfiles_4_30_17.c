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

#include "misc.h"

// These are now defined in misc.h
//#define DELIMITER !
//#define DELIMITER_CHAR '!'
//#define DELIMITER_STR "!"



char *hostname;
char *port;


int get_clientfd(char *hostname, char *port){
  int socketfd = -1;
  struct addrinfo hints;
  struct addrinfo *results;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG; //get number ports only
  hints.ai_socktype = SOCK_STREAM; //get socket connections only
  //hints.ai_family = AF_INET; //ipv4 or ipv6
  //printf("%s, %s\n", hostname, port);
  int returnCheck = getaddrinfo(hostname, port, &hints, &results);
  if(returnCheck != 0){
    //error check here
    fprintf(stderr, "Error with getaddrinfo, %s\n", gai_strerror(returnCheck));
  }
  else{
    struct addrinfo *ptr = results;
    while(ptr != NULL){
      socketfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if(socketfd < 0){
        //socket failed
        ptr = ptr->ai_next;
        continue;
      }
      if(connect(socketfd, ptr->ai_addr, ptr->ai_addrlen) < 0){
        //failed to connect to server
        ptr = ptr->ai_next;
        close(socketfd);
        continue;
      }
      break;
    }
    freeaddrinfo(results);
    if(ptr == NULL){
      //none of the addresses worked
      return -1;
    }
  }
  return socketfd;
}

int netopen(const char *pathname, int flags){
  //FILE *test = fopen("test.txt", "r");
  //char pathTest[10000];
  //fgets(pathTest, 10000, test);


  int clientfd;
  char *recievedMessage;
  char flags_[100];
  sprintf(flags_, "%d", flags);
  
  
  int sentMessageLen = strlen(pathname) + strlen(flags_) + strlen("o") + 3; // only count everything after the second delimeter
  char len[100];
  sprintf(len, "%d", sentMessageLen);
  char *sentMessage = malloc(sentMessageLen + 2 + strlen(len) + 1); // +2 for 5 delimeters total adding on from + 3 in sentMessageLen, two for each parameter, +1 for null terminating char
  //printf("%i %s %s", sentMessageLen, len, pathname);
  clientfd = get_clientfd(hostname, port);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return -1;
  }
  //build the message here
  
  const char *messageComponents[] = {"o", pathname, flags_};
  // MUST BE CONST

  char *messageToSend = prepMessage(messageComponents,
          sizeof(messageComponents)/sizeof(char*));
  printf("Will send message %s\n", messageToSend);
  printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
  write(clientfd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte

  /*
  strcat(sentMessage, DELIMITER_STR);
  strcat(sentMessage, len);
  strcat(sentMessage, DELIMITER_STR);
  strcat(sentMessage, "o");
  strcat(sentMessage, DELIMITER_STR);
  strcat(sentMessage, pathname);
  strcat(sentMessage, DELIMITER_STR);
  strcat(sentMessage, flags_);
  strcat(sentMessage, DELIMITER_STR);
  printf("Manual message: %s\n", sentMessage);
  printf("About to send message of length %lu bytes to server\n", sentMessageLen + 2 + strlen(len) + 1);
  */
  printf("Automatic message: %s\n", messageToSend);

  //printf("%s", sentMessage);
  //(size((o((pathname((flags(
  // write(clientfd, sentMessage, strlen(sentMessage) + 1); // +1 for the null byte

  // The below call waits for the server to post something to the file descriptor
  // Note that if the server is stuck in a readn loop waiting for bytes it's not
  // going to receive, it won't be able to post to the file descriptor until an
  // error occurs and readn exits (e.g. the client disconnects).
  recievedMessage = (char*)malloc(20);
  int r = read(clientfd, recievedMessage, 20);
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    return -1;
  }

  // MESSAGE LENGTH:
  //getting the size of the message aka the first parameter
  int bytesRead = 1; // number of bytes actually examined
  // first delimeter was already read in
  char *messageLen = (char*)malloc(1); 
  int currIndex = 0;
  int currentMaxSize = r; //number of bytes read in via read function


  // ********** TODO what if there's no new bytes to read?
   while(bytesRead < currentMaxSize){

    printf("Current character: '%c'\n", recievedMessage[bytesRead]);

    if(recievedMessage[bytesRead] == DELIMITER_CHAR){
      bytesRead++;
      break;
    }
    else if(recievedMessage[bytesRead] == '\0'){ //you've reached the end of the message
                                                 //without seeing the second delimieter yet
      printf("Bad message, not enough delimiters\n");
      break;
    }
    else{
      messageLen[currIndex] = recievedMessage[bytesRead];
      currIndex++;
      bytesRead++;
      char *updatedMessageLen = realloc(messageLen, bytesRead);
      if(updatedMessageLen != NULL) {
        // free(messageLen); DO NOT DO THIS!
        messageLen = updatedMessageLen;
      }
      //need to read 20 more bytes from the server 
      if(bytesRead == currentMaxSize){
        printf("About to realloc\n");
        char *updatedRecievedMessage = realloc(recievedMessage, currentMaxSize + 20);
        if(updatedRecievedMessage != NULL){
          //free(recievedMessage);
          recievedMessage = updatedRecievedMessage;
        }
        char tmp[20];
        int r = read(clientfd, tmp, 20);
        strcat(recievedMessage, tmp);

        // TODO handle errors
        currentMaxSize += r;
      }
    }
  }
  messageLen[currIndex] = '\0';
  printf("Message length: %s\n", messageLen);
  //recievedMessage[currentMaxSize - 1] = '\0';
  //start reading recievedMessage starting from the second delimiter
  //messageLen should be the number of bytes after the second delimiter and not including
  //the null terminating character?
  //
  // GET REST OF MESSAGE
  int lenint = atoi(messageLen);
  char *messageContent = malloc(lenint);
  memcpy(messageContent, recievedMessage + bytesRead, currentMaxSize - bytesRead);
  messageContent[currentMaxSize - bytesRead] = '\0'; // TODO test
  char restOfContent[lenint - (currentMaxSize - bytesRead)];
  read(clientfd, restOfContent, lenint - (currentMaxSize - bytesRead)); //how to ensure this happens?
  //purpose of getting the length is to minimize number of read calls
  // TODO solution: use readn
  
  strcat(messageContent, restOfContent);

  printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);
  //TODO return network file descriptor
  char *networkfiledescriptor = parseToken(messageContent, 0);
  int nfd = atoi(networkfiledescriptor);
  
  //memset(receivedMessage, 0, sizeof(receivedMessage));
  free(sentMessage);
  //TODO check for errno
  if(nfd == -1){

  }
  return nfd;
}
ssize_t netread(int fildes, void *buf, size_t nbyte){
  
}
int main(int argc, char **argv){
  /*int clientclientfd;
  char buffer[10000];
  char receivedMessage[10000];
  char sentMessage[10000];*/
  char buffer[10000];
  if(argc != 3){
    printf("Too few/many arguments\n");
    return 0;
  }
  hostname = argv[1];
  port = argv[2];
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
      netread(networkfd, buf, nBytes);
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

