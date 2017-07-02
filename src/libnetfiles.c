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

// These are now defined in misc.h
//#define DELIMITER !
//#define DELIMITER_CHAR '!'
//#define DELIMITER_STR "!"

#define INVALID_FILE_MODE 200

/*struct virtualSocket_{
  int in_use;
  int port;
};

typedef struct virtualSocket_ virtualSocket;*/

char *globalhostname;
char *globalport;
int globalfilemode;
int netserverinitwascalled = 0;
//int numOfSocketsAvailable = 10;
//virtualSocket availablePorts[10]; //starting from 10001 going to 10010
//int currentSocket = 0;


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

//netserverinit(char * hostname)
int netserverinit(char * hostname, int filemode) {
  // call get_clientfd and check return value
  int clientfd = get_clientfd(hostname, "10001");
  if(clientfd == -1){
    errno = HOST_NOT_FOUND; // TODO test this!
    close(clientfd);
    return -1;
  }
  if(filemode < 0 && filemode > 2){
    errno = INVALID_FILE_MODE;
    fprintf(stderr, "Invalid file mode!\n"); //TODO find better way to do
    close(clientfd);
    return -1;
  }
  globalhostname = hostname;
  globalport = "10001";
  globalfilemode = filemode;
  netserverinitwascalled = 1;
  close(clientfd);
  return 0;
}

int netopen(const char *pathname, int flags){
  if(flags != 0 && flags != 1 && flags != 2){
    fprintf(stderr, "Invalid flag entered in netopen!\n");
  }
  //FILE *test = fopen("test.txt", "r");
  //char pathTest[10000];
  //fgets(pathTest, 10000, test);
  if(netserverinitwascalled == 0){
    return -1;
  }
  int clientfd;
  char *recievedMessage; // TODO remember to free
  char flags_[100];
  char fileMode_[100];
  sprintf(flags_, "%d", flags);  
  sprintf(fileMode_, "%d", globalfilemode);  

  clientfd = get_clientfd(globalhostname, globalport);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return -1;
  }
  //build the message here
  
  const char *messageComponents[] = {"o", pathname, flags_, fileMode_};
  // MUST BE CONST

  char *messageToSend = prepMessage(messageComponents,
          sizeof(messageComponents)/sizeof(char*));
  //printf("Will send message %s\n", messageToSend);
  //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
  write(clientfd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
  free(messageToSend);

  //(size((o((pathname((flags(

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

    //printf("Current character: '%c'\n", recievedMessage[bytesRead]);

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
        //printf("About to realloc\n");
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
  //printf("Message length: %s\n", messageLen);
  
  // GET REST OF MESSAGE
  
  int len = atoi(messageLen);
  char *messageContent = malloc(len); // TODO remember to free this!
  messageContent[0] = '\0';
  int numBytesReadAfterDelimiter = currentMaxSize - bytesRead; // includes null byte
  memcpy(messageContent, recievedMessage + bytesRead, numBytesReadAfterDelimiter);
  //messageContent[numBytesReadAfterDelimiter] = '\0'; // TODO test
  char restOfContent[len - numBytesReadAfterDelimiter];
  restOfContent[0] = '\0';
  readn(clientfd, restOfContent, len - numBytesReadAfterDelimiter);
  //restOfContent[len-numBytesReadAfterDelimiter - 1] = '\0';
  strcat(messageContent, restOfContent);

  
  /*printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);*/

  char *networkfiledescriptor = parseToken(messageContent, 0); // TODO remember to free
  int nfd = atoi(networkfiledescriptor);
  
  if(nfd == -1) {
    int index = strlen(networkfiledescriptor) + 1;
    char *errorNumStr = parseToken(messageContent, index); // TODO remember to free
    int errNum = atoi(errorNumStr);
    printf("Setting errno to %d\n", errNum);
    errno = errNum; 
    free(errorNumStr);
  }
  free(messageLen);
  free(messageContent);
  free(networkfiledescriptor);
  free(recievedMessage);
  close(clientfd);			// TODO needed??
  return nfd;
}

ssize_t netread(int fildes, void *buf, size_t nbyte){

  if(netserverinitwascalled == 0){
    return -1;
  }
  int clientfd;
  char *recievedMessage; // TODO remember to free
  char nbyte_[100];
  char nfd_[100];
  sprintf(nbyte_, "%lu", nbyte);  
  sprintf(nfd_, "%d", fildes);

  clientfd = get_clientfd(globalhostname, globalport);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return -1;
  }
  //build the message here
  
  const char *messageComponents[] = {"r", nfd_, nbyte_};
  // MUST BE CONST

  char *messageToSend = prepMessage(messageComponents,
          sizeof(messageComponents)/sizeof(char*));
  //printf("Will send message %s\n", messageToSend);
  //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
  /*int numOfSocketsUsed = 0;
  int numBytesLeft = (int) nbyte;
  while(numBytesLeft > 0 && socketsAvailable > 0){
    int i;
    if(socketsAvailable == 1){
      for(i = 0; i < 10; i++){
        if(virtualSockets[i].in_use == 0){
          virtualSockets[i].in_use = 1;
          printf("%i bytes will be read on server port %i\n", numBytesLeft, virtualSockets[i].port);
          numOfSocketsUsed++; 
          break;
        }
      }
      break;
    } 
    else if(numBytesLeft <= 2000){
      for(i = 0; i < 10; i++){
        if(virtualSockets[i].in_use == 0){
          virtualSockets[i].in_use = 1;
          printf("%i bytes will be read on server port %i\n", numBytesLeft, virtualSockets[i].port);
          numOfSocketsUsed++; 
          break;
        }
      }
      break;
    }
    else{
      for(i = 0; i < 10; i++){
        if(virtualSockets[i].in_use == 0){
          virtualSockets[i].in_use = 1;
          printf("%i bytes will be read on server port %i\n", 2000, virtualSockets[i].port);
          numOfSocketsUsed++; 
          break;
        }
      }
      numBytesLeft -= 2000;
    }
  }
  if(numOfSocketsUsed== 0){
    printf("All ten sockets are currently in use, no available sockets at this time\n");
  }
  printf("Read operation will require %i sockets\n", numOfSocketsNeeded);
  while(numOfSocketsNeed > 0){
    printf("
  }*/
  write(clientfd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
  free(messageToSend);  

  // Deal with response message
  recievedMessage = (char*)malloc(20);
  recievedMessage[0] = '\0';
  //printf("RecievedMessage so far: %s", recievedMessage);
  int r = read(clientfd, recievedMessage, 20);
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    return -1;
  }

  // Get the length of the message (by extracting the first parameter)
  int bytesRead = 1;
  int currentMaxSize = r;
  char *messageLen = getMessageLength(&recievedMessage, clientfd, &bytesRead, 
                     &currentMaxSize);
  // TODO remember to free this!

  //printf("Message length: %s\n", messageLen);
  
  // GET REST OF MESSAGE
  int len = atoi(messageLen);
  char *messageContent = malloc(len); // TODO remember to free this!
  messageContent[0] = '\0';
  int numBytesReadAfterDelimiter = currentMaxSize - bytesRead; // includes null byte
  memcpy(messageContent, recievedMessage + bytesRead, numBytesReadAfterDelimiter);
  messageContent[numBytesReadAfterDelimiter] = '\0'; // TODO test
  char restOfContent[len - numBytesReadAfterDelimiter];
  restOfContent[0] = '\0';
  readn(clientfd, restOfContent, len - numBytesReadAfterDelimiter);
  //restOfContent[len-numBytesReadAfterDelimiter - 1] = '\0';
  strcat(messageContent, restOfContent);

  /*printf("NumBytesReadAfterDelimiter: %d\n", numBytesReadAfterDelimiter);
  printf("Rest of content: %s\n", restOfContent);
  printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);*/

  char *readRetVal_ = parseToken(messageContent, 0); // TODO remember to free
  int readRetVal = atoi(readRetVal_);
  int index = strlen(readRetVal_) + 1;
  if (readRetVal == -1) {
    char *errorNumStr = parseToken(messageContent, index); // TODO remember to free
    int errNum = atoi(errorNumStr);
    printf("Setting errno to %d\n", errNum);
    errno = errNum; 
    free(errorNumStr);
  }
  else {
    //printf("netread success. %d bytes were read.\n", readRetVal);
    char *readBytes = parseToken(messageContent, index); // TODO remember to free
    memcpy(buf, readBytes, readRetVal); // TODO null-terminate?
    //printf("Copied these bytes: %s\n", readBytes);
    free(readBytes);

  }

  free(readRetVal_);
  free(messageLen);
  free(messageContent);
  free(recievedMessage);
  close(clientfd);

  return readRetVal;
}


ssize_t netwrite(int fildes, const void *buf, size_t nbyte){

  if(netserverinitwascalled == 0) {
    return -1;
  }
  int clientfd;
  char *recievedMessage; // TODO remember to free
  char nbyte_[100];
  char nfd_[100];
  char messageToWrite[nbyte]; // NOTE: **NOT** null-terminated!
  
  //assuming buffer passed in by client is null-terminating
  //printf("buffer: %s\n", buf);
  memcpy(messageToWrite, buf, strlen(buf) + 1); // This really isn't necessary.
  // We instead should just pass buf as a component of the message to send
  // to the server.
  //assuming buffer passed in by client is null-terminating
  if(nbyte > strlen(messageToWrite)){
    nbyte = strlen(messageToWrite);
  }
  //printf("nbyte: %i\n", nbyte);
  sprintf(nbyte_, "%lu", nbyte);  
  sprintf(nfd_, "%d", fildes);
  clientfd = get_clientfd(globalhostname, globalport);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return -1;
  }
  //build the message here
  
  const char *messageComponents[] = {"w", nfd_, nbyte_, messageToWrite};
  // MUST BE CONST

  char *messageToSend = prepMessage(messageComponents,
          sizeof(messageComponents)/sizeof(char*));
  //printf("Will send message %s\n", messageToSend);
  //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
  write(clientfd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
  free(messageToSend);  

  // Deal with response message
  recievedMessage = (char*)malloc(20);
  recievedMessage[0] = '\0';
  //printf("RecievedMessage so far: %s", recievedMessage);
  int r = read(clientfd, recievedMessage, 20);
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    return -1;
  }

  // Get the length of the message (by extracting the first parameter)
  int bytesRead = 1;
  int currentMaxSize = r;
  char *messageLen = getMessageLength(&recievedMessage, clientfd, &bytesRead, 
                     &currentMaxSize);
  // TODO remember to free this!

  //printf("Message length: %s\n", messageLen);
  
  // GET REST OF MESSAGE
  int len = atoi(messageLen);
  char *messageContent = malloc(len); // TODO remember to free this!
  messageContent[0] = '\0';
  int numBytesReadAfterDelimiter = currentMaxSize - bytesRead; // includes null byte
  memcpy(messageContent, recievedMessage + bytesRead, numBytesReadAfterDelimiter);
  messageContent[numBytesReadAfterDelimiter] = '\0'; // TODO test
  char restOfContent[len - numBytesReadAfterDelimiter];
  restOfContent[0] = '\0';
  readn(clientfd, restOfContent, len - numBytesReadAfterDelimiter);
  //restOfContent[len-numBytesReadAfterDelimiter - 1] = '\0';
  strcat(messageContent, restOfContent);

  /*(printf("NumBytesReadAfterDelimiter: %d\n", numBytesReadAfterDelimiter);
  printf("Rest of content: %s\n", restOfContent);
  printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);*/

  char *writeRetVal_ = parseToken(messageContent, 0); // TODO remember to free
  int writeRetVal = atoi(writeRetVal_);
  int index = strlen(writeRetVal_) + 1;
  if (writeRetVal == -1) {
    char *errorNumStr = parseToken(messageContent, index); // TODO remember to free
    int errNum = atoi(errorNumStr);
    printf("Setting errno to %d\n", errNum);
    errno = errNum; 
    free(errorNumStr);
  }
  else {
    //printf("netwrite success. %d bytes were written.\n", writeRetVal);
  }

  free(writeRetVal_);
  free(messageLen);
  free(messageContent);
  free(recievedMessage);
  close(clientfd);

  return writeRetVal;
}


int netclose(int fd) {

  if(netserverinitwascalled == 0){
    return -1;
  }
  int clientfd;
  char *recievedMessage; // TODO remember to free
  char nfd_[100];
  sprintf(nfd_, "%d", fd);

  clientfd = get_clientfd(globalhostname, globalport);
  if(clientfd < 0){
    printf("Failed to connect to server, shutting down program\n");
    return -1;
  }
  //build the message here
  
  const char *messageComponents[] = {"c", nfd_};
  // MUST BE CONST

  char *messageToSend = prepMessage(messageComponents,
          sizeof(messageComponents)/sizeof(char*));
  //printf("Will send message %s\n", messageToSend);
  //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
  write(clientfd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
  free(messageToSend);  

  // Deal with response message
  recievedMessage = (char*)malloc(20);
  recievedMessage[0] = '\0';
  //printf("RecievedMessage so far: %s", recievedMessage);
  int r = read(clientfd, recievedMessage, 20);
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    return -1;
  }

  // Get the length of the message (by extracting the first parameter)
  int bytesRead = 1;
  int currentMaxSize = r;
  char *messageLen = getMessageLength(&recievedMessage, clientfd, &bytesRead, 
                     &currentMaxSize);
  // TODO remember to free this!

  //printf("Message length: %s\n", messageLen);
  
  // GET REST OF MESSAGE
  int len = atoi(messageLen);
  char *messageContent = malloc(len); // TODO remember to free this!
  messageContent[0] = '\0';
  int numBytesReadAfterDelimiter = currentMaxSize - bytesRead; // includes null byte
  memcpy(messageContent, recievedMessage + bytesRead, numBytesReadAfterDelimiter);
  messageContent[numBytesReadAfterDelimiter] = '\0'; // TODO test
  char restOfContent[len - numBytesReadAfterDelimiter];
  restOfContent[0] = '\0';
  readn(clientfd, restOfContent, len - numBytesReadAfterDelimiter);
  //restOfContent[len-numBytesReadAfterDelimiter - 1] = '\0';
  strcat(messageContent, restOfContent);

  /*printf("NumBytesReadAfterDelimiter: %d\n", numBytesReadAfterDelimiter);
  printf("Rest of content: %s\n", restOfContent);
  printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);*/

  char *closeRetVal_ = parseToken(messageContent, 0); // TODO remember to free
  int closeRetVal = atoi(closeRetVal_);
  int index = strlen(closeRetVal_) + 1;
  if (closeRetVal == -1) {
    char *errorNumStr = parseToken(messageContent, index); // TODO remember to free
    int errNum = atoi(errorNumStr);
    printf("Setting errno to %d\n", errNum);
    errno = errNum; 
    free(errorNumStr);
  }
  else {
    //printf("close success\n");

  }

  free(closeRetVal_);
  free(messageLen);
  free(messageContent);
  free(recievedMessage);
  close(clientfd);

  return closeRetVal;


}


int main2(int argc, char **argv){
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
      netread(networkfd, buf, bytes);
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

