#ifndef HELPERFUNCS_H
#define HELPERFUNCS_H

#define LISTENQ 5

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int open_clientfd(char *hostname, char *port);

int open_listenfd(char *port);

#endif
