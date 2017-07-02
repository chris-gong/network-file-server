
#include "misc.h"

ssize_t readn(int fd, void *usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char *bufp = usrbuf;
  //printf("Readn called\n");
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

    //printf("Bytes read: %d\n", nread);
    nleft -= nread;
    bufp += nread;
  }
  //printf("Readn is returning %lu\n", n-nleft);
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




// Gets the length of a message (actually, of the part after the length)
// by extracting its first parameter (whatever comes before the second delimiter character).
// Note that this could involve reading more bytes from the file
// descriptor fd and reallocating the buffer in which the message is stored
// This is why the message argument is a char**.
//
// bytesRead represents the number of bytes of the message which have been examined
// currentMaxSize represents the number of bytes of the message which have been
// read into the message via the read function
char *getMessageLength(char **message, int fd, int *bytesRead, int *currentMaxSize) {

  // getting the size of the message aka the first parameter
  // int bytesRead = 1; // number of bytes actually examined
  // first delimeter was already read in
  char *messageLen = (char*)malloc(1); // this is the buffer where the length will go
  int currIndex = 0;
  // int currentMaxSize = startSize; // number of bytes read in via read function


  // ********** TODO what if there's no new bytes to read?
   while(*bytesRead < *currentMaxSize){

    //printf("Current character: '%c'\n", (*message)[*bytesRead]);

    if ((*message)[*bytesRead] == DELIMITER_CHAR) {
      (*bytesRead)++; // advance past the delimiter
      break;
    }
    else if ((*message)[*bytesRead] == '\0') { //you've reached the end of the message
                                                 //without seeing the second delimieter yet
      printf("Bad message, not enough delimiters\n");
      break;
    }
    else {
      messageLen[currIndex] = (*message)[*bytesRead];
      currIndex++;
      (*bytesRead)++;
      char *updatedMessageLen = realloc(messageLen, *bytesRead);
      if (updatedMessageLen != NULL) {
        // free(messageLen); DO NOT DO THIS!
        messageLen = updatedMessageLen;
      }
      //need to read 20 more bytes from the file descriptor
      if(*bytesRead == *currentMaxSize) {
        // TODO it's not necessary to keep the old bytes
        //printf("About to realloc\n");
        char *updatedRecievedMessage = realloc(*message, *currentMaxSize + 20);
        if(updatedRecievedMessage != NULL) {
          //free(recievedMessage);
          *message = updatedRecievedMessage;
        }
        char tmp[20];
        int r = read(fd, tmp, 20);
        strcat(*message, tmp);

        // TODO handle errors
        currentMaxSize += r;
      }
    }
  }
  messageLen[currIndex] = '\0';

  return messageLen;

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
  boolean foundMatchingDelim = FALSE;

  while(message[count] != '\0'){
    //printf("Current char in message: %c\n", message[count]);
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

  // t e s t \0
  // tokenLen = 4

  if (foundMatchingDelim == FALSE) {
    //printf("Invalid message. No ending delimiter!\n");
    //return NULL; // TODO end connection due to bad message
    /*printf("No delimiter found, so copying up to null byte\n");
    printf("tokenLen is %d\n", tokenLen);*/
    token = malloc(tokenLen + 1);
    memcpy(token, message + startIndex, tokenLen);
    token[tokenLen] = '\0';
    //printf("** Returned token will be: %s **\n", token);
  }

  return token;
}

char *prepMessage(const char *messages[], int numMessages) {
  char *messageToSend = NULL;
  int count;
  int reportedMessageLength = 1; // begin at 1 to account for null byte
  int totalMessageLength = 0;
  char reportedMessageLengthStr[100];
 
   for (count = 0; count < numMessages; count++) {
    //printf(messages[count]);
    reportedMessageLength += strlen(messages[count]);
  }
  reportedMessageLength += 1*(numMessages-1); // the last one dosen't need a delimiter

  sprintf(reportedMessageLengthStr, "%d", reportedMessageLength);
  totalMessageLength = 2 + strlen(reportedMessageLengthStr) + reportedMessageLength;
  // 2 for the first two delimiters (which surround the message length portion)
  // + the length of the reported message length string
  // + the length of the remainder of the message (as given by reportedMessageLength)
  // NO  + 1 for the null byte (it's included already in the reportedMessageLength)
  
  //printf("The total length of the message to be sent is %d bytes\n", totalMessageLength);
  messageToSend = malloc(totalMessageLength);
  messageToSend[0] = '\0';
  if (messageToSend == NULL) {
    // error
    return NULL;
  }
  strcat(messageToSend, DELIMITER_STR);
  strcat(messageToSend, reportedMessageLengthStr);
  strcat(messageToSend, DELIMITER_STR);
  for (count = 0; count < numMessages-1; count++) {
    strcat(messageToSend, messages[count]);
    strcat(messageToSend, DELIMITER_STR);
  }
  strcat(messageToSend, messages[numMessages-1]);
  
  //printf("The message that will be sent is %s\n", messageToSend);
  return messageToSend;

}


