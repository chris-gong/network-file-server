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

#include "linkedlist.h"
#include "misc.h"


// These are now defined in misc.h
//#define DELIMITER !
//#define DELIMITER_CHAR '!'
//#define DELIMITER_STR "!"

//typedef enum {FALSE, TRUE} boolean;

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
  int *mode; // Unrestricted = 0, Exclusive = 1, Transaction = 2
};

struct mutex_{
  pthread_mutex_t *mx;
  char *pathname;
};

/*struct request_{

};*/
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

typedef struct mutex_ mutex;

typedef enum {OPEN, READ, WRITE, CLOSE, BAD_INSTRUCTION} InstructionType;


linkedlist *nfdList;
linkedlist *mutexes;
int nfdCounter = -2; // global nfd counter

pthread_mutex_t tableMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nfdCounterMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queueRequestMutex = PTHREAD_MUTEX_INITIALIZER;

/* **************************************************** 
 *   Functions relating to the file descriptor table  *
 * ****************************************************/

// Initializes a networkFileDescriptor struct. Note that the pathname is COPIED byte-by-byte.
networkFileDescriptor *initNfd(networkFileDescriptor *nfd, char *pathname,
    int filefd, int flags, int mode) {

  nfd->pathname = malloc(strlen(pathname) + 1);
  memcpy(nfd->pathname, pathname, strlen(pathname) + 1);
  nfd->filefd = malloc(sizeof(int));
  nfd->flags = malloc(sizeof(int));
  nfd->networkfd = malloc(sizeof(int));
  nfd->mode = malloc(sizeof(int));
  *(nfd->filefd) = filefd;
  *(nfd->flags) = flags;
  *(nfd->networkfd) = nfdCounter;
  *(nfd->mode) = mode;
  pthread_mutex_lock(&nfdCounterMutex);
  // Need to use a mutex since we're modifying a global variable
  nfdCounter--;
  pthread_mutex_unlock(&nfdCounterMutex);

  return nfd;
}



void printNetworkFileDescriptor(networkFileDescriptor *nfd) {
  printf("Pathname: %s\n", nfd->pathname);
  printf("filefd: %d\n", *(nfd->filefd));
  printf("flags: %d\n", *(nfd->flags));
  printf("networkfd: %d\n", *(nfd->networkfd));
  printf("mode: %d\n", *(nfd->mode));
}

// Could make this into a function in the linked list file itself.
// Such a function would loop over the nodes and then call some sort of
// user-specified function on each node's data variable.
void printNfdLL(linkedlist *list) {
  llnode *curr = list->head;
  if (curr == NULL) {
    printf("List is empty.\n");
    return;
  }
  networkFileDescriptor *nfd;
  while (curr != NULL) {
    nfd = (networkFileDescriptor *) curr->data;
    printNetworkFileDescriptor(nfd);
    printf("-----------------------------------\n");

    curr = curr->next;
  }

}

int myCompare(void *a, void *b) {
  networkFileDescriptor *fd_1 = (networkFileDescriptor *) a;
  int *fd_2 = (int *) b;

  //printf("Comparing %d to %d\n", *(fd_1->networkfd), *(fd_2));

  if(*(fd_1->networkfd) < *(fd_2)) {
    return -1;
  }
  else if(*(fd_1->networkfd) > *(fd_2)) {
    return 1;
  }
  else{
    return 0;
  }
}
void mutexDeallocateDataMembers(llnode *node){
  mutex *data = (mutex *)node->data;
  free(data->pathname);
  free(data->mx);
}

void myDeallocateDataMembers(void *nfd_arg) {
  //printf("Inside nfd struct data member deallocation function\n");
  networkFileDescriptor *data = (networkFileDescriptor *)nfd_arg;
  close(*(data->filefd));
  free(data->pathname);
  free(data->filefd);
  free(data->flags);
  free(data->networkfd);
  free(data->mode);
}

boolean isOpenInTransaction(char *filepath) {

  // loop over the file descriptor table.
  // If there is an entry with pathname equal to filepath
  // AND its mode is TRANSACTION, return TRUE
  llnode *current = nfdList->head;
  networkFileDescriptor *nfd;
  boolean result = FALSE;

  pthread_mutex_lock(&tableMutex);
  while (current != NULL) {
    nfd = (networkFileDescriptor *)current->data;
    if (strcmp(nfd->pathname, filepath) == 0 && *(nfd->mode) == TRANSACTION) {
      result = TRUE;
      break;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&tableMutex);
  return result;
}
boolean isOpenAtAll(char *filepath) {

  // loop over the file descriptor table.
  // If there is an entry with pathname equal to filepath
  // AND its mode is TRANSACTION, return TRUE
  llnode *current = nfdList->head;
  networkFileDescriptor *nfd;
  boolean result = FALSE;

  pthread_mutex_lock(&tableMutex);
  while (current != NULL) {
    nfd = (networkFileDescriptor *)current->data;
    if (strcmp(nfd->pathname, filepath) == 0) {
      result = TRUE;
      break;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&tableMutex);
  return result;
}

boolean isOpenWithWritePermissions(char *filepath) {

  // loop over the file descriptor table.
  // If there is an entry with pathname equal to filepath
  // AND its mode is TRANSACTION, return TRUE
  llnode *current = nfdList->head;
  networkFileDescriptor *nfd;
  boolean result = FALSE;

  pthread_mutex_lock(&tableMutex);
  while (current != NULL) {
    nfd = (networkFileDescriptor *)current->data;
    if (strcmp(nfd->pathname, filepath) == 0 && (*(nfd->flags) == O_WRONLY || *(nfd->flags) == O_RDWR)) {
      result = TRUE;
      break;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&tableMutex);
  return result;
}

boolean isOpenInExclusiveWithWritePermissions(char *filepath) {

  // loop over the file descriptor table.
  // If there is an entry with pathname equal to filepath
  // AND its mode is TRANSACTION, return TRUE
  llnode *current = nfdList->head;
  networkFileDescriptor *nfd;
  boolean result = FALSE;

  pthread_mutex_lock(&tableMutex);
  while (current != NULL) {
    nfd = (networkFileDescriptor *)current->data;
    if (strcmp(nfd->pathname, filepath) == 0 && *(nfd->mode) == EXCLUSIVE && (*(nfd->flags) == O_WRONLY || *(nfd->flags) == O_RDWR)) {
      result = TRUE;
      break;
    }
    current = current->next;
  }
  pthread_mutex_unlock(&tableMutex);
  return result;
}


pthread_mutex_t mutexTableMutex;

/* **************************************************** 
 *   Functions relating to the mutex table            *
 * ****************************************************/


int mutexComparePathName(void *a, void *b) {
  mutex *mx = (mutex *) a;
  char *target = (char *) b;

  return strcmp(mx->pathname, target);

}

// NOTE: assumes mtx is already initialized!
mutex *initMutex(mutex *m, pthread_mutex_t *mtx, char *pathname) {
  m->pathname = malloc(strlen(pathname) + 1);
  memcpy(m->pathname, pathname, strlen(pathname) + 1);
  m->mx = malloc(sizeof(pthread_mutex_t));
  *(m->mx) = *mtx; // copy the mutex
  return m;
}

void deallocateMutexStructMembers(void *mutex_arg) {
  mutex *m = (mutex *)mutex_arg;
  free(m->pathname);
  free(m->mx);

}

/* **************************************************** 
 *   Server-related helper functions                  *
 * ****************************************************/



int get_listenfd(char *port) {
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
  recievedMessage[0] = '\0';
  int r = read(fd, recievedMessage, 20); // Do **NOT** use readn here. We don't know if the message is shorter than 20 chars, so we don't want to block the program/make it wait for 19
  if (r == 0) { // No message. Close.
    close(*(targs.connectionfd));
    free(targs.connectionfd);
    free(arguments);
    free(recievedMessage);
    return NULL;
  }
  //printf("Message so far: %s\n", recievedMessage);

  // the first char should be a delimeter
  if (recievedMessage[0] != DELIMITER_CHAR) {
    // TODO error: malformed message
    printf("Error with message format\n");
    writeMessage(fd, "Malformed message");
    close(*(targs.connectionfd));
    free(targs.connectionfd);
    free(arguments);
    free(recievedMessage);
    return NULL;

  }

  // Get the length of the message (by extracting the first parameter)
  int bytesRead = 1;
  int currentMaxSize = r;
  char *messageLen = getMessageLength(&recievedMessage, fd, &bytesRead, &currentMaxSize);
  // TODO remember to free this!

  // At this point, bytesRead contains the index of the char after the
  // delimiter after the message length
  
  //printf("Message length: %s\n", messageLen);

  // Getting remainder of message

  // ! 8 ! t e s t a b c \0
  //       3   5
  
  // bytesRead = 3 (we've examined !, 8, and !)
  // currentMaxSize = 6 (we've read 6 bytes total at this point)
  // numBytesReadAfterDelimiter = 6-3 = 3

  // messageContent:
  // t e s \0
  
  // restOfContent (size 8-3 = 5):
  // t a b c \0

  //messageContent:
  //t e s t a b c \0
 
  // Potential problem:

  //! 5 ! - 1 ! 9 ! \0
  //      3
  // currentMaxSize = 9
  // bytesRead = 3
  // numBytesReadAfterDelimiter = 9-3 = 6
  // len = 5
  // len - numBytesReadAfterDelimiter = 5-6 = -1
  // The reason this happens is that len doesn't accout for the null byte
  // at the end of the message

  // As of 4/30/17, this has been fixed, because now len DOES account for the null
  // byte at the end of the message

  // GET REST OF MESSAGE
  int len = atoi(messageLen);
  char *messageContent = malloc(len); // TODO remember to free this!
  messageContent[0] = '\0';
  int numBytesReadAfterDelimiter = currentMaxSize - bytesRead; // includes null byte
  memcpy(messageContent, recievedMessage + bytesRead, numBytesReadAfterDelimiter);
  messageContent[numBytesReadAfterDelimiter] = '\0'; // TODO test
  char restOfContent[len - numBytesReadAfterDelimiter];
  restOfContent[0] = '\0';
  readn(fd, restOfContent, len - numBytesReadAfterDelimiter); //how to ensure this happens?
  //purpose of getting the length is to minimize number of read calls
  // TODO solution: use readn
  
  strcat(messageContent, restOfContent);

  /*printf("Current max size: %d\n", currentMaxSize);
  printf("Bytes read: %d\n", bytesRead);
  printf("Message content: %s\n", messageContent);*/

  char *function = parseToken(messageContent, 0); // TODO make sure to free this!
  //printf("function: %s\n", function);
  int functionLen = strlen(function);
  int index = 0 + functionLen + 1; // add 1 to skip the delimiter
  
  //   _f_ _u_ _n_ _c_ _!_ _b_ _l_ _a_
  //    ^               ^
  //    0              0+4


  // Response message
  char *messageToSend;

  // Error-related stuff
  char *error_return = "-1";
  char error_buffer[5] = {0}; // TODO what if we don't zero it out?
  const char **errorMessageComponents = NULL;
  int error_num = 0;
 
  // For netopen:
  char *filePath = NULL; // TODO make sure to free this!
  char *flags = NULL;    // TODO make sure to free this!
  char *mode = NULL;     // TODO make sure to free this!
  
  int filePathLen = 0;
  int flagsLen = 0;
  int nfdLen = 0;
  int flagsInt = -1;
  int modeInt = -1;

  int newFd = 0;
  networkFileDescriptor localNfd; // Not a pointer!
  llnode *newNode = NULL;

  // For netread, netwrite, and netclose:
  char *nfdStr = NULL;	 // TODO make sure to free this!

  // For netread and netwrite:
  char *nbytes = NULL;   // TODO make sure to free this!
  int nfd = -1;
  int bytes = -1;

  llnode *mxnode;
  llnode *networkfdnode;
  mutex *foundMutex;
  networkFileDescriptor *foundNfd;
  int nfdFlags = -1;

  // For netread
  char *readBuff = NULL; // TODO make sure to free this!
  int readResult = 0;
  char readResultStr[100] = {0}; // to hold the return value of readn

  // For netwrite
  char *writeBuff = NULL; // TODO make sure to free this!
  int writeResult = 0;
  char writeResultStr[100] = {0}; // to hold the return value of writen

  // For netclose
  int closeResult = 0;
  char closeResultStr[100] = {0}; // to hold the return value of close

  InstructionType type = getInstructType(function);
  switch (type) {
    case OPEN:
      filePath = parseToken(messageContent, index);
      if (filePath == NULL) { // invalid message
		// TODO return an error that the message was invalid!
      }
      //first check if file path already has a mutex in the linked list
      pthread_mutex_lock(&mutexTableMutex);
      mxnode = llsearch(mutexes, filePath, mutexComparePathName);
      pthread_mutex_unlock(&mutexTableMutex);
      //if we don't then we add
      if (mxnode == NULL) {
        llnode *tmpNode = malloc(sizeof(llnode)); // Do not make this local!
        pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
        mutex fileMutex;
        initMutex(&fileMutex, &mx, filePath);
        initllnode(tmpNode, &fileMutex, sizeof(fileMutex));
        mxnode = tmpNode;

        pthread_mutex_lock(&mutexTableMutex);
        llinsert(mutexes, mxnode);
        pthread_mutex_unlock(&mutexTableMutex);
      }
 
      //printf("File path: %s\n", filePath);
      filePathLen = strlen(filePath);
      index = index + filePathLen + 1;
      flags = parseToken(messageContent, index);
      if (flags == NULL) { // invalid message
		// TODO return an error that the message was invalid!
      }
      //printf("Flags: %s\n", flags);
      index = index + strlen(flags) + 1;
      mode = parseToken(messageContent, index);
      //printf("%s\n", mode);
      if (mode == NULL) { // invalid message

		// TODO return an error that the message was invalid!
      }

      flagsInt = atoi(flags);
      modeInt = atoi(mode);
      //printf("Mode: %d\n", modeInt);
      //printf("filePath %s flagsInt %i", filePath, flagsInt);
      // Extension A logic goes here.
      // should make functions:
      // isOpenInTransaction(char *filepath)
      // isOpenAtAll(char *filepath)
      // isOpenWithWritePermissions(char *filepath)
      // isOpenInExclusiveWithWritePermissions(char *filepath)
      // REMEMBER TO LOCK/UNLOCK MUTEX IN THESE FUNCTIONS

      // Then call these functions according to the flow chart
      if (isOpenInTransaction(filePath) == TRUE) {
        //printf("This file is open in transaction mode.\n");
        //only one nfd open at all if in trasaction mode
        printf("Can't open file since this file has already been opened by a client in transaction mode.\n");
        //send message back to client
        errno = EACCES; //permission denied
      } 
      else {
        //printf("This file is not open in transaction mode.\n");
        if(modeInt == TRANSACTION && isOpenAtAll(filePath) == TRUE){
          printf("Can't open a file in transaction mode when the file has already been opened by other users.\n");
          //send message back to client
          errno = EACCES; //permission denied
        }
        else{
          if(modeInt == EXCLUSIVE && (flagsInt == O_WRONLY || flagsInt == O_RDWR)){
            if(isOpenWithWritePermissions(filePath) == TRUE){
              printf("Can't open a file in exclusive mode when the file has already been opened with write permissions.\n");
              errno = EACCES; //permission denied
            }
          }
          else{
            if(modeInt == UNRESTRICTED && (flagsInt == O_WRONLY || flagsInt == O_RDWR) && isOpenInExclusiveWithWritePermissions(filePath) == TRUE){
              printf("Can't open a file in write mode when the file has already been opened in exclusive mode with write permissions.\n");
              errno = EACCES; //permission denied
            } 	
          }
        }
        
      }
      
      //TODO check if file is a directory, in this case return an error
      //TODO check for the 5 errors listed in the assignment description
      
      newFd = open(filePath, flagsInt);
      if (newFd == -1 || errno == EACCES || errno == EINTR || errno == EISDIR|| errno == ENOENT || errno == EROFS || errno == EPERM) { // error opening file
        perror("netopen");

        sprintf(error_buffer, "%d", errno);
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

      }
      else {
        //printf("Successfully opened the file. The descriptor is %d\n", newFd);
        newNode = malloc(sizeof(llnode));
        initNfd(&localNfd, filePath, newFd, flagsInt, modeInt);
        //printf("Sizeof nfd is %lu\n", sizeof localNfd);
        initllnode(newNode, &localNfd, sizeof localNfd);
        // Mutex needs to go here since we're modifying global data structures!
        pthread_mutex_lock(&tableMutex);
        llinsert(nfdList, newNode);
        pthread_mutex_unlock(&tableMutex);

        // No need to free localNfd; it's a local variable
        // DO NOT free the node here. It is needed for the linked list!
        // DO NOT free the malloced data members of localNfd e.g. localNfd->pathname,
        // localfd->filefd, etc. These memory areas are needed by the byte-by-byte copy
        // of the localNfd struct pointed to by the node.
        
        // If the node is deleted from the list, it will be freed, along with
        // the nfd struct it points to (which is a byte-by-byte copy of localNfd)
        // Additionally, the malloced data members of said nfd struct will be freed.

        //writeMessage(fd, "!14!success and test 1234567!");
        // "!<LEN>!<NETWORKFD>!
        char networkFdString[100];
        sprintf(networkFdString, "%d", *(localNfd.networkfd));
        const char *openSuccessMessageComponents[] = {networkFdString};

        /*printf("Number of compoments in this message: %lu\n", 
          sizeof(openSuccessMessageComponents)/sizeof(char *));*/
        messageToSend = prepMessage(openSuccessMessageComponents, 
          sizeof(openSuccessMessageComponents)/sizeof(char*)); 
        /*printf("Will send message %s\n", messageToSend);
        printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));*/
        write(fd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
        free(messageToSend);
      }
      free(filePath); // this we should free, since the nfd struct made a COPY of it.
      free(flags);

      break;

    case READ:
      // message: r!<nfd>!<bytes>!
      nfdStr = parseToken(messageContent, index); // index is set at this point
      if (nfdStr == NULL) {
		// TODO return error for invalid message
      }
      index += strlen(nfdStr) + 1;
      nbytes = parseToken(messageContent, index);
      if (nbytes == NULL) {
		// TODO return error for invalid message
      }
      bytes = atoi(nbytes);
      nfd = atoi(nfdStr);
      //first check if nfd entered by user is valid/exists
      //printf("HEAD NODE: %p\n", nfdList->head);
      //networkFileDescriptor *tmp = (networkFileDescriptor *)nfdList->head->data;
      //printf("HEAD NODE NETWORKFILEFD: %d\n", *(tmp->networkfd));
      pthread_mutex_lock(&tableMutex);
      networkfdnode = llsearch(nfdList, &nfd, myCompare);
      pthread_mutex_unlock(&tableMutex);
      if (networkfdnode == NULL) {
        printf("ERROR: Network file descriptor not found!\n");
        errno = EBADF;
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);
        
        free(nbytes);
        free(nfdStr); 
        break;
        //return NULL; //send a message back to client saying nfd is bad
      }
      foundNfd = (networkFileDescriptor *) networkfdnode->data;
      // check read permission on this file descriptor:
      nfdFlags = *(foundNfd->flags);
      if (nfdFlags != O_RDONLY && nfdFlags != O_RDWR) {
        // permission error!
        printf("ERROR: File descriptor lacks permission to read!\n");
        errno = EACCES;
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);
        
        free(nbytes);
        free(nfdStr);
        break;
      }

      char *readFilePath = foundNfd->pathname; 
      //printf("Searching for mutex\n");

      pthread_mutex_lock(&mutexTableMutex);
      mxnode = llsearch(mutexes, readFilePath, mutexComparePathName); // TODO TEST!!!!
      // Check for null?
      foundMutex = (mutex *) mxnode->data;
      pthread_mutex_unlock(&mutexTableMutex);
      //for now, mx shouldn't be null because we're gonna play safe and not delete
      //from mutexes linkedlist
//      printf("Found mutex with pathname %s\n", foundMutex->pathname);
//      printf("****About to read (%d bytes) on server side*****\n", bytes);   
      readBuff = malloc(bytes+1); // + 1 for null byte at end
      pthread_mutex_lock(foundMutex->mx);
      //read stuff, call readn
      readResult = readn(*(foundNfd->filefd), readBuff, bytes);
      readBuff[bytes] = '\0';
      pthread_mutex_unlock(foundMutex->mx);
      if (readResult == -1) {
        perror("read");
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

      }
      else { // read was successful
        sprintf(readResultStr, "%d", readResult);
        //printf("Read was successful. Content read: %s\n", readBuff);
        const char *readSuccessMessageComponents[] = {readResultStr, readBuff};

        messageToSend = prepMessage(readSuccessMessageComponents, 2); 
        //printf("Will send message %s\n", messageToSend);
        //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
        write(fd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
        free(messageToSend);
      }
      free(readBuff);
      free(nbytes);
      free(nfdStr);
      break;
    case WRITE: // TODO do we include a null byte when we write?
      // message: w!<nfd>!<bytes>!<whatToWrite>
      nfdStr = parseToken(messageContent, index); // index is set at this point
      if (nfdStr == NULL) {
		// TODO return error for invalid message
      }
      index += strlen(nfdStr) + 1;
      nbytes = parseToken(messageContent, index);
      if (nbytes == NULL) {
		// TODO return error for invalid message
      }
      index += strlen(nbytes) + 1;
      writeBuff = parseToken(messageContent, index);
      //printf("***** The bytes that will be written are: %s\n", writeBuff);

      bytes = atoi(nbytes);
      nfd = atoi(nfdStr);

      //first check if nfd entered by user is valid/exists
      pthread_mutex_lock(&tableMutex);
      networkfdnode = llsearch(nfdList, &nfd, myCompare);
      pthread_mutex_unlock(&tableMutex);
      if (networkfdnode == NULL) {
        printf("ERROR: Network file descriptor not found!\n");
        errno = EBADF;
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

        free(writeBuff);        
        free(nbytes);
        free(nfdStr); 
        break;
        //return NULL; //send a message back to client saying nfd is bad
      }
      foundNfd = (networkFileDescriptor *) networkfdnode->data;
      // check write permission on this file descriptor:
      nfdFlags = *(foundNfd->flags);
      if (nfdFlags != O_WRONLY && nfdFlags != O_RDWR) {
        // permission error!
        printf("ERROR: File descriptor lacks permission to write!\n");
        errno = EACCES;
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

        free(writeBuff);        
        free(nbytes);
        free(nfdStr);
        break;
      }

      char *writeFilePath = foundNfd->pathname; 
      //printf("Searching for mutex\n");

      pthread_mutex_lock(&mutexTableMutex);
      mxnode = llsearch(mutexes, writeFilePath, mutexComparePathName); // TODO TEST!!!!
      // Check for null?
      foundMutex = (mutex *) mxnode->data;
      pthread_mutex_unlock(&mutexTableMutex);
      //for now, mx shouldn't be null because we're gonna play safe and not delete
      //from mutexes linkedlist
      
      pthread_mutex_lock(foundMutex->mx);
      //write stuff
      writeResult = writen(*(foundNfd->filefd), writeBuff, bytes);
      pthread_mutex_unlock(foundMutex->mx);
      if (writeResult == -1) {
        perror("write");
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

      }
      else { // write was successful
        sprintf(writeResultStr, "%d", writeResult);
        //printf("Write was successful. Wrote: %d bytes.\n", writeResult);
        const char *writeSuccessMessageComponents[] = {writeResultStr};

        messageToSend = prepMessage(writeSuccessMessageComponents, 1); 
        //printf("Will send message %s\n", messageToSend);
        //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
        write(fd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
        free(messageToSend);
      }
      free(writeBuff);
      free(nbytes);
      free(nfdStr);


      break;
    case CLOSE:
      // message: c!<nfd>!
      nfdStr = parseToken(messageContent, index); // index is set at this point
      if (nfdStr == NULL) {
		// TODO return error for invalid message
      }

      nfd = atoi(nfdStr);

      //first check if nfd entered by user is valid/exists
      pthread_mutex_lock(&tableMutex);
      networkfdnode = llsearch(nfdList, &nfd, myCompare);
      pthread_mutex_unlock(&tableMutex);
      if (networkfdnode == NULL) {
        printf("ERROR: Network file descriptor not found!\n");
        errno = EBADF;
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

        free(nfdStr); 
        break;
        //return NULL; //send a message back to client saying nfd is bad
      }
      foundNfd = (networkFileDescriptor *) networkfdnode->data;
      closeResult = close(*(foundNfd->filefd));
      lldeletenode(nfdList, networkfdnode);
      // TODO if after removing this file descriptor struct, there are no more
      // associated with this file path, then remove the corresponding mutex
      // struct from the linked list


      if (closeResult == -1) {
        perror("write");
        sprintf(error_buffer, "%d", errno);
        
        errorMessageComponents = malloc(2*sizeof(char*));     
        errorMessageComponents[0] = error_return;
        errorMessageComponents[1] = error_buffer;
        messageToSend = prepMessage(errorMessageComponents, 2);
        write(fd, messageToSend, strlen(messageToSend) + 1);
        //printf("Wrote %lu bytes.\n", strlen(messageToSend) + 1);
        free(errorMessageComponents);
        free(messageToSend);

      }
      else { // close was successful
        sprintf(closeResultStr, "%d", closeResult);
        //printf("Close was successful.\n");
        const char *closeSuccessMessageComponents[] = {closeResultStr};

        messageToSend = prepMessage(closeSuccessMessageComponents, 1); 
        //printf("Will send message %s\n", messageToSend);
        //printf("strlen(messageToSend) = %lu\n", strlen(messageToSend));
        write(fd, messageToSend, strlen(messageToSend) + 1); // +1 for the null byte
        free(messageToSend);
      }
      free(nfdStr);


      break;


      break;
    case BAD_INSTRUCTION:
      break;
  }
  free(messageLen);
  free(function);
  free(messageContent);
  free(recievedMessage);

  //writeMessage(fd, "success message here...");
  close(*(targs.connectionfd));
  free(targs.connectionfd);
  free(arguments);
  return NULL;
}



int main(int argc, char **argv){
  // networkfds = malloc(sizeof(networkFileDescriptor) * numOfNetworkfds);
  int listenfd;
  pthread_t client_tid;

  // Create and initialize the linked list of network file descriptor structs
  nfdList = malloc(sizeof(linkedlist));
  mutexes = malloc(sizeof(linkedlist));
  llinit(nfdList);
  llinit(mutexes);
  nfdList->deallocateDataMembers = myDeallocateDataMembers;
  //printf("Sizeof linked list struct is %lu\n", sizeof(linkedlist));


  /*if(argc != 2){
    //error check
    printf("Too few/many arguments\n");
    return 0;
  }*/
  listenfd = get_listenfd("10001");
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

  free(nfdList);
  return 0;
}
