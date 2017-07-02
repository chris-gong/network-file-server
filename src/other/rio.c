#include "rio.h"

ssize_t rio_readn(int fd, void *usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char *bufp = usrbuf;

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

    nleft -= nread;
    bufp += nread;
  }

  return n-nleft; // will be >= 0

}

ssize_t rio_writen(int fd, void *usrbuf, size_t n) {
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

// initialize a rio read buffer
void rio_readinitb(rio_t *rp, int fd) {
  rp->rio_fd = fd;
  rp->rio_cnt = 0;
  rp->rio_bufptr = rp->rio_buf;
}

// NOTE: This is a private function.
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
  int cnt;
  while (rp->rio_cnt <= 0) { // refill if the buffer is empty
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf)); // could get a short count here
    if (rp->rio_cnt < 0) {
      if (errno != EINTR) { // interrupted by sig handler return
        return -1;
      }
    }
    else if (rp->rio_cnt == 0) { // EOF
      return 0;
    }
    else {
      rp->rio_bufptr = rp->rio_buf; // reset buffer ptr
    }
  } // end while loop

  // copy min(n, rp->rio_cnt) bytes from internal buf to user buf
  cnt = n;
  if (rp->rio_cnt < n) {
    cnt = rp->rio_cnt;
  }
  memcpy(usrbuf, rp->rio_bufptr, cnt);
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;
  return cnt;

}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
  int n, rc;
  char c, *bufp = usrbuf;

  for (n = 1; n < maxlen; n++) {
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      *bufp++ = c;
      if (c == '\n') {
        n++;
        break;
      }
    } else if (rc == 0) {
      if (n == 1) {
        return 0; // EOF and no data read
      }
      else {
        break; // EOF, but some data was read
      }
    } else {
      return -1; // error
    }
  } // end for loop

  *bufp = 0; // TODO what is this doing?
  return n-1;

}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char *bufp = usrbuf;

  while (nleft > 0) {
    if ((nread = rio_read(rp, bufp, nleft)) < 0) {
      return -1; // errno set by read()
    } else if (nread == 0) {
      break; // EOF
    }
    nleft -= nread;
    bufp += nread;

  } // end while loop
  return n-nleft; // will be >= 0
}

