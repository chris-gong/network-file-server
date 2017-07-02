#ifndef MISC_H
#define MISC_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

typedef enum {FALSE, TRUE} boolean;

#define DELIMITER_CHAR '!'
#define DELIMITER_STR "!"

#define UNRESTRICTED 0
#define EXCLUSIVE 1
#define TRANSACTION 2

#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

// TODO what happens if we leave these out?

ssize_t readn(int, void *, size_t);

ssize_t writen(int, void *, size_t);

char *getMessageLength(char **, int, int *, int *);

char *parseToken(char *, int);

char *prepMessage(const char *[], int);




#endif
