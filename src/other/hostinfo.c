#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char **argv) {
  struct addrinfo *p, *listp, hints;
  char buf[200];
  int rc, flags;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
    exit(0);
  }

  // Get list of addrinfo records
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;		// Limit to IPv4
  hints.ai_socktype = SOCK_STREAM;	// Limit to things we can connect to

  if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
    exit(1);
  }

  // Walk the linked list and display the IP addresses
  flags = NI_NUMERICHOST; // show address string instead of domain name
  for (p = listp; p; p = p->ai_next) {
    getnameinfo(p->ai_addr, p->ai_addrlen, buf, 200, NULL, 0, flags);
    printf("%s\n", buf);
  }

  // clean up
  freeaddrinfo(listp);

 exit(0);

}
