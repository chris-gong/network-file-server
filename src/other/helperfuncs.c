#include "helperfuncs.h"


int open_clientfd(char *hostname, char *port) {
  int clientfd;
  struct addrinfo hints, *listp, *p;

  // hints contains parameters for the call to getaddrinfo
  // listp is a pointer to the head node of the linked list
  // resulting from the call to getaddrinfo
  
  // Get list of potential server addresses/sockets
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM; // restrict results to addresses we can connect to
  hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
  int res = getaddrinfo(hostname, port, &hints, &listp);
  printf("RESULT OF GETADDRINFO: %d\n", res);
  // traverse the list for an address/socket we can successfully connect to
  for (p = listp; p; p = p->ai_next) {
    // create a socket descriptor
    if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      continue; // try the next address/socket
    }
    
    // connect to server
    if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) {
      break; // connection successful
    }
    close(clientfd); // connection failed; move on to next address/socket

  }

  // clean up
  freeaddrinfo(listp);
  if (!p) { // all connections failed
    return -1;
  } else {
    return clientfd;
  }

}

int open_listenfd(char *port) {
  struct addrinfo hints, *listp, *p;
  int listenfd, optval = 1;

  // Get list of potential server addresses
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM; // accept connections
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; // on any IP address
  hints.ai_flags |= AI_NUMERICSERV; // using a port number
  getaddrinfo(NULL, port, &hints, &listp);

  // Traverse the list to find a socket we can bind to
  for (p = listp; p; p = p->ai_next) {
    // create a socket descriptor
    if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      continue; // error with this socket; try the next one
    }

    // Remove the "address already in use" error from bind
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, // TODO study more about this
      (const void *)&optval, sizeof(int));

    // bind the descriptor to the address
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
      break; // success
    }
    close(listenfd); // bind failure; try next socket

  }
  
  // clean up
  freeaddrinfo(listp);
  if (!p) { // all connect attempts failed; no address worked
    return -1;
  }

  // make the socket listening and ready to accept requests
  if (listen(listenfd, LISTENQ) < 0) { // failure
    close(listenfd);
    return -1; // TODO shouln't we go to the next socket in the linked list?
    // i.e. put this code before the freeaddrinfo call so we can continue looping if
    // this listen call fails
  }

  return listenfd;

}



