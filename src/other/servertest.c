#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "helperfuncs.h"
#include "rio.h"

void echo(int connfd) {
  size_t n;
  char buf[2048];
  rio_t rio;

  rio_readinitb(&rio, connfd);
  while ((n = rio_readlineb(&rio, buf, 2048)) != 0) {
    printf("Server received %d bytes\n", (int) n);
    rio_writen(connfd, buf, n);
  }

}

int main(int argc, char **argv) {
  int fd_conn;
  int fd_listen = open_listenfd("3555");
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  char client_host[2048], client_port[256];

  char buf[200] = {0};
  memcpy(buf, "Test", 4); // could also use strcpy
  // strcpy(buf, "Test", 4);

  printf("Server listening on port 3555\n");
  while(1) {
    clientlen = sizeof(struct sockaddr_storage); // TODO why in the loop?
    fd_conn = accept(fd_listen, (struct sockaddr *) &clientaddr, &clientlen);
    getnameinfo((struct sockaddr *) &clientaddr, clientlen, client_host, 2048,
        client_port, 256, 0);
    printf("Connected to (%s, %s)\n", client_host, client_port);
    write(fd_conn, buf, 5);
    //echo(fd_conn);
    close(fd_conn); // commenting it out prevents broken pipe error
    printf("Connection closed.\n");
  }


  //printf("%d\n", fd_listen);
  exit(0);
}
