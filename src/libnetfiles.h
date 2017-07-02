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

#ifndef LIBNETFILES_H
#define LIBNETFILES_H

int netopen(const char *pathname, int flags);

ssize_t netread(int fildes, void *buf, size_t nbyte);

ssize_t netwrite(int fildes, const void *buf, size_t nbyte);

int netclose(int fd);

int netserverinit(char * hostname, int filemode);

#endif
