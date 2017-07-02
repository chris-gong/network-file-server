#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <signal.h>

#include "helperfuncs.h"
#include "rio.h"

//typedef void *(signalhandler_t)(int);

void handler(int arg) {
  puts("Problem");
}

int main(int argc, char **argv) {
  int clientfd;
  char *host, *port;
  char buf[2048];
  rio_t rio;

  signal(SIGPIPE, handler);

  // validate args here

  host = "adapter.cs.rutgers.edu";
  port = "3555";

  clientfd = open_clientfd(host, port);
  rio_readinitb(&rio, clientfd);

  while (fgets(buf, 2048, stdin) != NULL) {
    printf("About to send message to server\n");
    if (rio_writen(clientfd, buf, strlen(buf)) <= 0) { // TODO null byte?
      printf("stuff %s\n", buf);
      perror("Error: ");
    }
    else {
      printf("Sent message to server successfully.\n");
    }

    //rio_readlineb(&rio, buf, 2048);
    if (rio_readn(clientfd, buf, strlen(buf)) <= 0) {
      printf("Couldn't read message from server.\n");
    }
    else {
      printf("Contents of buffer (could be message from you or from server): '%s'", buf);
    }
  }
  printf("Out of while loop\n");
  
  //printf("%d\n", fd_listen);
  close(clientfd);
  exit(0);
}
