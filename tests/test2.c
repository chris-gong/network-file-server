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

#include "src/libnetfiles.h"
#include "src/misc.h"




int main(int argc, char **argv){
 
  char buffer[1000000];
  char readBuff[1000000];
 
  netserverinit("localhost", 0);
  
  int nfd = netopen("big.txt", O_RDWR);
  int count;
  for (count = 0; count < 10000; count++) {
    memcpy(buffer+10*count, "abcdefghij", 10);
  }

  //int result = netwrite(nfd, buffer, 100000);
  //printf("Buffer: %s\n", buffer);
  //printf("Result of netwrite: %d\n", result);

  int result = netread(nfd, readBuff, 100000);
  //printf("Result of netread: %d\n", result);
  printf("%s", readBuff);

  netclose(nfd);

  
  return 0;
}
