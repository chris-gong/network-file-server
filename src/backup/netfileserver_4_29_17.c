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
#include <pthread.h>

#define DELIMITER !
#define DELIMITER_CHAR '!'
#define DELIMITER_STR "!"

typedef enum {FALSE, TRUE} boolean;

struct thread_Arguments{
  char *message;
  int *connectionfd;
  //char recievedMessage[10000];
  char *recievedMessage;
  int reveivedMessageLength;
  char sentMessage[10000]; // TODO needed?
};

struct network_File_Descriptor {
  char *pathname;
  int *filefd;
  int *flags; //O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2
  int *networkfd;
};

int nfdCounter; // global nfd counter


/*
 * Structure of the old table:
 *
 * | size:         |
 * | numEntries:   |    ____________________________________________________________
 * | nfdCounter:   |    |							   |
 * | data:---------|--> |           Continuous region for 10 nfd structs           |
 * |               |    |            (or alternatively, 10 pointers to nfds)       |
 * |               |    |	     (in that case, this region would be smaller)  |
 *                      ------------------------------------------------------------
 * 
 * Structure of the new table (linked-list based):
 *
 * | size:         |
 * | numEntries:   |    ____________    ____________     ___________
 * | nfdCounter:   |    |	   |    |	   |    |	   | 
 * | data:---------|--> |   nfd    |--> |   nfd    |--> |   nfd    |
 * |               |    |          |    |	   |    |	   | 
 * |               |    |	   |    |	   |    |	   | 
 *                      ------------    ------------    ------------
 * 
 * 
 *
 *
 */

typedef struct thread_Arguments threadArguments;

typedef struct network_File_Descriptor networkFileDescriptor;


typedef enum {OPEN, READ, WRITE, CLOSE, BAD_INSTRUCTION} InstructionType;

//int numOfNetworkfds = 0;

networkFileDescriptor *networkfds;

//int networkfdcounter = -2;


networkFileDescriptor *initNfd(networkFileDescriptor *nfd) {

  return NULL;
}


ssize_t readn(int fd, void *usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char *bufp = usrbuf;
  printf("Readn called\n");
  while (nleft > 0) {
    if ((nread = read(fd, bufp, nleft)) < 0) {
      if (errno == EINTR) { // sig handler return
        nread = 0; // we don't want nread to be -1
      }
      else {
        return -1; // read() set errno
      }
    }
    else if (nread == 0) {
      break; // EOF
    }

    printf("Bytes read: %d\n", nread);
    nleft -= nread;
    bufp += nread;
  }
  printf("Readn is returning %lu\n", n-nleft);
  return n-nleft; // will be >= 0

}

ssize_t writen(int fd, void *usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nwritten;
  char *bufp = usrbuf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) { // TODO why <= 0
      if (errno == EINTR) { // sig handler return
        nwritten = 0; // we don't want nread to be -1
      }
      else {
        return -1; // write() set errno
      }
    }

    nleft -= nwritten;
    bufp += nwritten;
  }

  return n;

}

int get_listenfd(char * port){
  struct addrinfo hints;
  struct addrinfo *results;
  int optval = 1;
  int socketfd = -1;
  memset(&hints, 0, sizeof(hints)); //this line is actually necessary
  hints.ai_family = AF_INET; //not specific to ipv4 or ipv6 addresses
  hints.ai_socktype = SOCK_STREAM; //get connections/sockets only
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG; //server-side socket, number ports only
  int returnCheck = getaddrinfo(NULL, port, &hints, &results);
  if(returnCheck != 0){
    //error check here
    fprintf(stderr, "Error with getaddrinfo, %s\n", gai_strerror(returnCheck));
  }
  else{
    struct addrinfo *ptr = results;
    while(ptr != NULL){
      socketfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if(socketfd == -1){
        fprintf(stderr, "Error with socket, Could not create socket file descriptor");
        ptr = ptr->ai_next;
        continue;
      }
      else{
        if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1){
          fprintf(stderr, "Error with setsockopt, Could not set socket options");
          close(socketfd);
          ptr = ptr->ai_next;
          continue;
        }
        if(bind(socketfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
          fprintf(stderr, "Error with bind, Could not bind socket to server");
          close(socketfd);
          ptr = ptr->ai_next;
          continue;
        }
        if(listen(socketfd, 100) < 0){ //limit queue to hold 100 connections most at one time
	  fprintf(stderr, "Error with listen, Could not make socket a listening socket");
	  close(socketfd);	
          ptr = ptr->ai_next;
          continue;
        }
      }
      break;
    }
    freeaddrinfo(results);
    if(ptr == NULL){
      return -1; //none of the addresses worked
    }
  }
  return socketfd;

}

int writeMessage(int fd, char *msg) { // TODO it may not be necessary to use a tmp buffer!
    int numBytesWritten;
    char *msgBuffer = malloc(strlen(msg) + 1);
    memcpy(msgBuffer, msg, strlen(msg) + 1);
    msgBuffer[strlen(msg)] = '\0';
    numBytesWritten = writen(fd, msgBuffer, strlen(msg) + 1);
    free(msgBuffer);
    // TODO if writen returns an error, we need to deal with it

    return numBytesWritten;

}

// Gets the type of instruction (open, close, read, write)
// from a string of the form "open", "close", etc.
InstructionType getInstructType(char *str) {
  if (strcmp(str, "o") == 0) {
    return OPEN;
  }
  else if (strcmp(str, "c") == 0) {
    return CLOSE;
  }
  else if (strcmp(str, "r") == 0) {
    return READ;
  }
  else if (strcmp(str, "w") == 0) {
    return WRITE;
  }
  else { // command unknown
    // TODO set errno
    return BAD_INSTRUCTION;
    
  }

}

// Parses a token (chars up to the first delimiter) from the message passed in.
// NOTE: message[startIndex] must not be a delimiter!
// Note that this function does not do any network reading; the message should
// already exist in the buffer passed into this function.
char *parseToken(char *message, int startIndex) {
  
  int tokenLen = 0;
  char *token;
  
  if (message[startIndex] == DELIMITER_CHAR) {
    printf("Invalid message. First char can't be a delimiter!\n");
    return NULL; // TODO end connection due to bad message
  }
  
  int count = startIndex;
  boolean foundMatchingDelim;

  while(message[count] != '\0'){
    printf("Current char in message: %c\n", message[count]);
    if (message[count] == DELIMITER_CHAR){
      
      token = malloc(tokenLen + 1);
      memcpy(token, message + startIndex, tokenLen);
      token[tokenLen] = '\0';
      foundMatchingDelim = TRUE;
      break;
    }
    else {
      tokenLen++;
    }
    count++;
  }

  if (foundMatchingDelim == FALSE) {
    printf("Invalid message. No ending delimiter!\n");
    return NULL; // TODO end connection due to bad message
  }

  return token;
}

void handleOpen() {

}

void *connectionHandler(void *args) {
  pthread_detach(pthread_self());
  threadArguments * arguments = args; // TODO typecast needed?
  threadArguments targs = *arguments;
  int fd = *(targs.connectionfd);

  char *recievedMessage;
  char *sentMessage;
  // TODO make sure the recievedMessage is null-terminated

  // read the message till we find the first matching delimiter
  recievedMessage = (char*)malloc(20);
  //recievedMessage[0] = '\0'; //typo?
  int r = read(fd, recievedMessage, 20); // Do **NOT** use readn here. We don't know if the message is shorter than 20 chars, so we don't want to block the program/make it wait for 19

  printf("Message: %s\n", recievedMessage);

  // the first char should be a delimeter
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    writeMessage(fd, "Malformed message");
    close(*(targs.connectionfd));
    free(targs.connectionfd);
    free(arguments);
    return NULL;


  }
  //getting the size of the message aka the first parameter
  int bytesRead = 1; // number of bytes actually examined
  // first delimeter was already read in
  char *messageLen = (char*)malloc(1); 
  int currIndex = 0;
  int currentMaxSize = r; //number of bytes read in via read function

  // TODO what if there's no new bytes to read?
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
        free(messageLen);
        messageLen = updatedMessageLen;
      }
      //need to read 20 more bytes from the server 
      if(bytesRead == currentMaxSize){
        // TODO it's not necessary to keep the old bytes
        printf("About to realloc\n");
        char *updatedRecievedMessage = realloc(recievedMessage, currentMaxSize + 20);
        if(updatedRecievedMessage != NULL){
          free(recievedMessage);
          recievedMessage = updatedRecievedMessage;
        }
        char tmp[20];
        int r = read(fd, tmp, 20);
        strcat(recievedMessage, tmp);

        //int r = read(fd, recievedMessage, 1);
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
  int len = atoi(messageLen);
  char *messageContent = malloc(len);
  memcpy(messageContent, recievedMessage + bytesRead, currentMaxSize - bytesRead);
  messageContent[currentMaxSize - bytesRead] = '\0'; // TODO test
  char restOfContent[len - (currentMaxSize - bytesRead)];
  read(fd, restOfContent, len - (currentMaxSize - bytesRead)); //how to ensure this happens?
  //purpose of getting the length is to minimize number of read calls
  // TODO solution: use readn
  
  //rest of parsing is just repeating the above loop basically
  strcat(messageContent, restOfContent);
  //printf("%c\n", recievedMessage + bytesRead);

  printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);

  /* int currentDelimiter = 2;
  
  bytesRead = 0;
  int paramLen = 0;
  int lastDelimiterIndex = 0;
  char *function;
  while(bytesRead < len){
    printf("Current char is messageContent: %c\n", messageContent[bytesRead]);
    if(messageContent[bytesRead] == DELIMITER_CHAR){
      
      if(currentDelimiter == 2){
        function = malloc(paramLen + 1);
        memcpy(function, messageContent + lastDelimiterIndex, paramLen);
        function[paramLen] = '\0';
        currentDelimiter++;
        break;
      }
      paramLen = 0;
      lastDelimiterIndex = bytesRead; 
    }
    else if(messageContent[bytesRead] == '\0'){
      
    }
    else{
      paramLen++;
      
    }
    bytesRead++;
  } */
  char *function = parseToken(messageContent, 0);
  printf("function: %s\n", function);
  int functionLen = strlen(function);
  int index = 0 + functionLen + 1; // add 1 to skip the delimiter
  
  //   _f_ _u_ _n_ _c_ _!_ _b_ _l_ _a_
  //    ^               ^
  //    0              0+4

  //check function value here and do more similar loops to get
  //rest of parameters
 
  char *filePath = NULL;
  char *flags = NULL;
  int filePathLen = 0;
  int flagsLen = 0;

  int flagsInt = -1;

  int newFd = 0;
  int newNetworkFd = 0;

  int error_num = 0;

  InstructionType type = getInstructType(function);
  switch (type) {
    case OPEN:
      filePath = parseToken(messageContent, index);
      if (filePath == NULL) { // invalid message

      }
      printf("File path: %s\n", filePath);
      filePathLen = strlen(filePath);
      index = index + filePathLen + 1;
      flags = parseToken(messageContent, index);
      if (flags == NULL) { // invalid message

      }
      printf("Flags: %s\n", flags);
      flagsInt = atoi(flags);
      /*if (strcmp(flags, "O_RDONLY") == 0) {
        flagsInt = O_RDONLY;
      } else if (strcmp(flags, "O_WRONLY") == 0) {
        flagsInt = O_WRONLY;
      } else if (strcmp(flags, "O_RDWR") == 0) {
        flagsInt = O_RDWR;
      } else { // invalid flags

      }
      */
      //TODO check if file is a directory, in this case return an error
      newFd = open(filePath, flagsInt);
      if (newFd == -1) { // error opening file
        perror("netopen");
      }
      else {
        printf("Successfully opened the file. The descriptor is %d\n", newFd);
        if (newFd == 1) newNetworkFd = -2000; // TODO careful; what if this already exists?
        newNetworkFd = -1 * newFd;
        writeMessage(fd, "success");
        /* 
        int i;
        for(i = 0; i < numOfNetworkfds; i++){
          if(*(networkfds[i].in_use) == 0){
            break;
          }
        }
        //WHEN CLOSING NETWORK FD, DON'T FREE in_use AND networkid
        if(i != numOfNetworkfds){ //there is an available space in the table to fit network fd info
	  //set network info of element in array that is no longer in use
          networkfds[i].pathname = malloc(strlen(filePath) + 1);
          memcpy(networkfds[i].pathname, filePath, strlen(filePath) + 1); // TODO check!!
          networkfds[i].filefd = malloc(sizeof(int));
          networkfds[i].flags = malloc(sizeof(int));
          *(networkfds[i].filefd) = newFd;
          *(networkfds[i].in_use) = 1;
          *(networkfds[i].flags) = flagsInt;
          //send *(networkfds[i].networkfd)
        }
        else{
          numOfNetworkfds++;
          networkFileDescriptor *temp = realloc(networkfds, sizeof(networkFileDescriptor) * numOfNetworkfds);
          if(temp != NULL){
            free(networkfds);
            //set network info of a new networkfd struct
            networkfds[i-1].pathname = malloc(sizeof(filePath));
            networkfds[i-1].pathname = filePath;
            networkfds[i-1].filefd = malloc(sizeof(int));
            networkfds[i-1].flags = malloc(sizeof(int));
            networkfds[i-1].in_use = malloc(sizeof(int));
            networkfds[i-1].networkfd = malloc(sizeof(int));
            *(networkfds[i-1].filefd) = newFd;
            *(networkfds[i-1].in_use) = 1;
            *(networkfds[i-1].flags) = flagsInt;
            *(networkfds[i-1].networkfd) = networkfdcounter;
            networkfdcounter--; //prepare networkfdcounter for next networkfd struct to be created
            //send *(networkfds[i].networkfd)
          }
          else{
            //error
          }
        }
        */
      }

      break;

    case READ:
      break;
    case WRITE:
      break;
    case CLOSE:
      break;
    case BAD_INSTRUCTION:
      break;
  }
  free(function);
  free(messageContent);
  free(recievedMessage);



  

  /*
  int recievedLen = strlen(targs.recievedMessage);
  memcpy(targs.sentMessage, "The server has recieved your message: ", strlen("The server has recieved your message: "));
  strcat(targs.sentMessage, targs.recievedMessage);
  int sentLen = strlen(targs.sentMessage);
  //sentMessage[sentLen - 1] = '\0';
  //printf("%s\n", arguments->sentMessage);
  //printf("connection file descriptor: %i\n", *(arguments->connectionfd));
  write(*(targs.connectionfd), targs.sentMessage, strlen(targs.sentMessage));
  memset(targs.recievedMessage, 0, strlen(targs.recievedMessage));
  memset(targs.sentMessage, 0, strlen(targs.sentMessage));

  */

  writeMessage(fd, "success message here...");
  close(*(targs.connectionfd));
  free(targs.connectionfd);
  free(arguments);
  return NULL;
}

int main(int argc, char **argv){
  // networkfds = malloc(sizeof(networkFileDescriptor) * numOfNetworkfds);
  int listenfd;
  //char recievedMessage[10000];
  //char sentMessage[10000];
  //printf("%c\n", DELIMITER_CHAR);
  //int testfd = open("test.txt", O_RDONLY);
  //if (testfd == -1) perror("open");
  pthread_t client_tid;

  // Create and initialize the linked list of network file descriptor structs




  if(argc != 2){
    //error check
    printf("Too few/many arguments\n");
    return 0;
  }
  listenfd = get_listenfd(argv[1]);
  if(listenfd < 0){
    printf("Failed to create server, shutting down program\n");
    return 0;
  }
  printf("Server is waiting for connections\n");
  while(1){
    struct sockaddr_in client_address;
    socklen_t clientlen= sizeof(client_address);
    int connectionfd = accept(listenfd, (struct sockaddr*) &client_address, &clientlen);
    threadArguments *args = malloc(sizeof(threadArguments));
    args->connectionfd = malloc(sizeof(int));
    *(args->connectionfd) = connectionfd;
    //printf("New connection\n");
   
    // TODO this needs to be done in the thread function!
    // read(connectionfd, args->recievedMessage, sizeof(args->recievedMessage));
    
    //printf("connection file descriptor: %i and %i\n", connectionfd, *(args->connectionfd));
    pthread_create(&client_tid, NULL, connectionHandler, (void *) args);
    //close(connectionfd);
  }
  return 0;
}
