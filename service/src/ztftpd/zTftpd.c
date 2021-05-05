#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "zTftpd.h"

enum {
      DEBUG_VERBOSE,
      DEBUG_INFO,
      DEBUG_ERROR,
      DEBUG_CRITICAL
};

#define DBGBIT(x) (1<< DEBUG_##x)
static int debuglevel = \
  DBGBIT(INFO) | DBGBIT(ERROR) | DBGBIT(CRITICAL);

#define DBG(level, fmt, ...) do { \
  if (debuglevel & DBGBIT(level)) \
    fprintf(stderr, "zTFTPd: " fmt, ## __VA_ARGS__); \
  } while (0)

char *gBasedir;
uint16_t gPort;

// child only globals
int gSockFd = 0;

char gRequestFile[516];

int gWindowSizeSet = 0;
int gWindowSize = 1;

int gBlkSizeSet = 0;
int gBlkSize = 512;


void cleanupChild(int sig) {
  int status;
  wait(&status);
}

ssize_t sendAck(int sockfd, uint16_t block) {
  tftpMessage a;
  ssize_t c;

  a.opcode = htons(ACK);
  a.contents.ack.block = htons(block);

  if ((c = send(sockfd, &a, 4, 0)) < 0) {
    DBG(ERROR, "Failed to send in sendAck %s\n", strerror(errno));
  }

  return c;
}

ssize_t sendError(int sockfd, uint16_t code, char *error, struct sockaddr_in *sin, socklen_t slen) {
  tftpMessage e;
  ssize_t c;

  e.opcode = htons(ERROR);
  e.contents.error.code = htons(code);

  if (strlen(error) > sizeof(e.contents.error.string)) {
    DBG(ERROR, "Error string too large, rejecting\n");
    return -1;
  }

  strcpy((char *)e.contents.error.string, error);

  if (sin == NULL) {
    if ((c = send(sockfd, &e, 4 + strlen(error), 0)) < 0) {
      DBG(ERROR, "Failed to send in sendError\n");
    }
  }
  else {
    if ((c = sendto(sockfd, &e, 4 + strlen(error), 0,
		    (struct sockaddr *) sin, slen)) < 0) {
      DBG(ERROR, "Failed to sendto in sendError\n");
    }
  }

  return c;
}

// this func also determines which options were set
ssize_t sendOack(int sockfd, struct sockaddr_in *sin, socklen_t slen) {
  tftpMessage o;
  ssize_t c;

  o.opcode = htons(OACK);

  ssize_t offset = 0;
  if (gBlkSizeSet) {
    strcpy((char *)o.contents.oack.options + offset, "blksize"); offset += 8;
    itoa(gBlkSize, (char *)o.contents.oack.options + offset);
    offset += strlen((char *)o.contents.oack.options + offset) + 1;
  }

  if (gWindowSizeSet) {
    strcpy((char *)o.contents.oack.options + offset, "windowsize"); offset += 11;
    itoa(gWindowSize, (char *)o.contents.oack.options + offset);
    offset += strlen((char *)o.contents.oack.options + offset) + 1;
  }

  if ((c = sendto(sockfd, &o, 2 + offset, 0,
		  (struct sockaddr *) sin, slen)) < 0) {
    DBG(ERROR, "Failed to sendto in sendOack\n");
  }

  return c;
}

ssize_t recvMesg(int sockfd, tftpMessage *m, struct sockaddr_in *sin, socklen_t *slen)
{
  ssize_t c;

  if ((c = recvfrom(sockfd, m, sizeof(*m), 0, (struct sockaddr *) sin, slen)) < 0
      && errno != EAGAIN) {
    DBG(ERROR, "Recvfrom failed with %s\n", strerror(errno));
  }

  return c;
}

ssize_t parseIntValue(char *request, char *end, int *dest) {
  if (request >= end) {
    return -1;
  }

  char *optEnd = NULL;
  if ((optEnd = memchr(request, '\0', end - request))) {
    *dest = atoi(request);
  } else {
    return -1;
  }

  return (optEnd - request) + 1;
}

ssize_t parseWindowSize(char *request, char *end) {

  if (request + 11 >= end) {
    return -1;
  }

  ssize_t p = 0;
  if (!strcasecmp(request, "windowsize")) {
    if ((p = parseIntValue(request + 11, end, &gWindowSize)) < 0) {
      DBG(ERROR, "Malformed options found\n");
      return -1;
    }
    DBG(VERBOSE, "Got windowsize %d\n", gWindowSize);
    return p + 11;
  }
  return p;
}

ssize_t parseBlockSize(char *request, char *end) {

  if (request + 8 >= end) {
    return -1;
  }

  ssize_t p = 0;
  if (!strcasecmp(request, "blksize")) {
    if ((p = parseIntValue(request + 8, end, &gBlkSize)) < 0) {
      DBG(ERROR, "Malformed options found\n");
      return -1;
    }
    DBG(VERBOSE, "Got blksize %d\n", gBlkSize);
    return p + 8;
  }
  return p;
}

int parseOptions(char *request, size_t len) {

  ssize_t p = 0;
  char *origRequest = request;
  char *end = request + len;
  while (request < end) {
    // only on success do we continue on
    if ((p = parseWindowSize(request, end)) > 0) {
      gWindowSizeSet = 1;
      request += p;
      continue;
    }
    if ((p = parseBlockSize(request, end)) > 0) {
      gBlkSizeSet = 1;
      request += p;
      continue;
    }
    break;
  }

  return request != origRequest;
}

struct iovec *prepareIov(size_t windowSize, size_t blkSize) {
  struct iovec *out;

  out = calloc(windowSize, sizeof(struct iovec));
  if (out == NULL) {
    return NULL;
  }

  blkSize += 4;

  int i = 0;
  for(i = 0; i < windowSize; i++) {
    // each block must include enough room for a header
    out[i].iov_base = malloc(blkSize);
    if (!out[i].iov_base) {
      goto err;
    }
    out[i].iov_len = blkSize;
  }

  return out;
 err:
  for(i = 0; i < windowSize; i++) {
    if (out[i].iov_base) {
      free(out[i].iov_base);
    }
  }
  free(out);
  return NULL;
}

void freeIov(struct iovec *iov, size_t windowSize) {
  int i = 0;
  for (i = 0; i< windowSize; i++) {
    if (iov[i].iov_base) {
      free(iov[i].iov_base);
    }
  }

  free(iov);
}

int handleRrq() {
  int fd;
  int ret = 1;
  struct iovec *iovs = NULL;

  fd = open(gRequestFile, O_RDONLY);
  if (fd < 0) {
    DBG(ERROR, "Could not open requested file\n");
    sendError(gSockFd, 0, "File was not found", NULL, 0);
    return 1;
  }

  iovs = prepareIov(gWindowSize, gBlkSize);
  if (!iovs) {
    DBG(ERROR, "Failed to allocate output\n");
    goto err;
  }

  int window = 0;
  int lastBlock = 0;
  uint16_t blockCnt;
  for (window = 0; !lastBlock; window++) {
    tftpMessage incoming;
    blockCnt = 0;
    for (blockCnt = 0; blockCnt < gWindowSize && !lastBlock; blockCnt++) {
      tftpMessage *outgoing = iovs[blockCnt].iov_base;
      ssize_t blen = 0;

      outgoing->opcode = htons(DATA);
      // block counts are off-by-one
      outgoing->contents.data.block = htons(window * gWindowSize + blockCnt + 1);

      if ((blen = read(fd, outgoing->contents.data.data, gBlkSize)) < gBlkSize) {
	// if read were to error we bail
	if (blen < 0) {
	  DBG(ERROR, "Read from file failed\n");
	  goto err;
	}

	// we can trash this because the file is done being read from
	iovs[blockCnt].iov_len = 4 + blen;
	lastBlock = 1;
      }

    }

    if (writev(gSockFd, iovs, blockCnt) < 0) {
      DBG(ERROR, "Send message failed to writeout\n");
      goto err;
    }

    if (recvMesg(gSockFd, &incoming, NULL, NULL) < 2) {
      DBG(ERROR, "Receive message failed in handleRrq\n");
      sendError(gSockFd, 0, "Invalid message received back", NULL, 0);
      goto err;
    }

    uint16_t opcode = ntohs(incoming.opcode);
    if (opcode == ERROR) {
      DBG(ERROR, "Got error back from remote\n");
      goto err;
    }

    if (opcode == ACK) {
      if (ntohs(incoming.contents.ack.block) != window * gWindowSize + blockCnt) {
	DBG(ERROR, "Got bad ack back from remote\n");
	sendError(gSockFd, 0, "Invalid ACK", NULL, 0);
	goto err;
      }
    } else {
      DBG(ERROR, "Received invalid opcode %d, from remote during block send\n", opcode);
      sendError(gSockFd, 0, "Invalid opcode sent", NULL, 0);
      goto err;
    }
  }

  ret = 0;
 err:
  close(fd);
  if (iovs)
    freeIov(iovs, gWindowSize);

  return ret;
}

int handleWrq() {
  int fd;
  int ret = 1;
  struct iovec *iovs = NULL;

  fd = open(gRequestFile, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
  if (fd < 0) {
    DBG(ERROR, "Could not create requested file\n");
    sendError(gSockFd, 0, "File could not be created", NULL, 0);
    return 1;
  }


  // According to RFC 2347 we don't ack if we sent an OACK ealier
  if (!(gBlkSizeSet || gWindowSizeSet)) {
    if (sendAck(gSockFd, 0) < 0) {
      DBG(ERROR, "Failed to send first ack in WRQ\n");
      goto err;
    }
  }

  iovs = prepareIov(gWindowSize, gBlkSize);
  if (!iovs) {
    DBG(ERROR, "Failed to allocate input\n");
    goto err;
  }

  int window;
  int lastBlock = 0;
  uint16_t blockCnt;
  for (window = 0; !lastBlock; window++) {
    ssize_t len = 0;
    blockCnt = 0;

    // receive a window
    for (; blockCnt < gWindowSize && !lastBlock; blockCnt++) {
      tftpMessage *incoming = iovs[blockCnt].iov_base;

      if (len == 0) {
	if ((len = readv(gSockFd, &iovs[blockCnt], gWindowSize - blockCnt)) < 4) {
	  DBG(ERROR, "Receive message failed in handleWrq\n");
	  sendError(gSockFd, 0, "Invalid message received", NULL, 0);
	  goto err;
	}
      }

      if (len < iovs[blockCnt].iov_len) {
	lastBlock = 1;
      }

      uint16_t opcode = ntohs(incoming->opcode);
      if (opcode == ERROR) {
	DBG(ERROR, "Got back error from remote\n");
	goto err;
      }

      if (opcode == DATA) {
	if (ntohs(incoming->contents.data.block) != (window * gWindowSize) + blockCnt + 1) {
	  DBG(ERROR, "Invalid block number received\n");
	  goto err;
	}

	size_t writeLen = iovs[blockCnt].iov_len < len ?
	  iovs[blockCnt].iov_len : len;

	if (write(fd, incoming->contents.data.data, writeLen - 4) < 0) {
	  DBG(ERROR, "Failed to write to PUT file\n");
	  goto err;
	}
      } else {
	DBG(ERROR, "Received invalid opcode %d, from remote during block recv\n", opcode);
	sendError(gSockFd, 0, "Invalid opcode sent", NULL, 0);
	goto err;
      }

      len -= iovs[blockCnt].iov_len;
    }

    if (sendAck(gSockFd, window * gWindowSize + blockCnt) < 0) {
      DBG(ERROR, "Failed to send follow-up ack in WRQ\n");
      goto err;
    }
  }


  ret = 0;
 err:
  close(fd);
  if (iovs)
    freeIov(iovs, gWindowSize);

  return ret;
}

int handle(tftpMessage *handshake, size_t hlen, struct sockaddr_in *sin, socklen_t slen) {
  int modeOff;
  struct timeval tv;

  // close the server sock fd
  close(gSockFd);
  if ((gSockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    DBG(ERROR, "Failed to allocate socket for client handler\n");
    return 1;
  }

  // set socket timeout here
  tv.tv_sec  = 8;
  tv.tv_usec = 0;

  if (setsockopt(gSockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    DBG(ERROR, "Failed to set timeout on socket\n");
    return 1;
  }

  if (connect(gSockFd, (struct sockaddr *)sin, slen) < 0) {
    DBG(ERROR, "Failed to connect\n");
    return 1;
  }

  int opcode = ntohs(handshake->opcode);

  memcpy(gRequestFile, handshake->contents.request.filename, hlen - sizeof(uint16_t));

  modeOff = strlen(gRequestFile) + 1;
  if (modeOff > (hlen - sizeof(uint16_t))) {
    DBG(ERROR, "Cound not find transfer mode\n");
    sendError(gSockFd, 0, "Mode not specified", sin, slen);
    return 1;
  }

  char *mode = gRequestFile + modeOff;
  if (strlen(mode) > hlen - sizeof(uint16_t) - modeOff) {
    DBG(ERROR, "Mode was determined to be the wrong size\n");
    sendError(gSockFd, 0, "Mode was an invalid size", sin, slen);
    return 1;
  }

  if (strchr(gRequestFile, '/')) {
    DBG(ERROR, "Requested file contained invalid characters\n");
    sendError(gSockFd, 0, "Invalid filename", sin, slen);
    return 1;
  }

  int modeValid = strcasecmp(mode, "netascii") ? 1 :
    strcasecmp(mode, "octet") ? 1 :
    0;
  
  if (!modeValid) {
    DBG(ERROR, "Invalid mode specified\n");
    sendError(gSockFd, 0, "Invalid mode", sin, slen);
    return 1;
  }
  
  DBG(INFO, "%s request from %s.%u for %s %s\n",
      ntohs(handshake->opcode) == RRQ ? "GET" : "PUT",
      inet_ntoa(sin->sin_addr), ntohs(sin->sin_port),
      gRequestFile, mode);

  // if options were parsed successfully we need to send an OACK
  if (parseOptions(gRequestFile + \
		   modeOff + \
		   strlen(mode) + 1,
		   hlen)) {

    if (gWindowSize < 1 || gWindowSize > 1024) {
      gWindowSize = DEFAULT_WINDOW_SIZE;
    }

    if (gBlkSize < 1 || gBlkSize > 2048) {
      gBlkSize = DEFAULT_BLK_SIZE;
    }

    if (sendOack(gSockFd, sin, slen) < 0) {
      DBG(ERROR, "Failed to send OAck\n");
      return 1;
    }
  }

  int ret = 0;
  if (opcode == RRQ) {
    ret = handleRrq(gSockFd, sin, slen);
  } else if (opcode == WRQ) {
    ret = handleWrq(gSockFd, sin, slen);
  } else {
    DBG(ERROR, "Impossible opcode sent in\n");
  }

  DBG(VERBOSE, "Transfer done\n");

  close(gSockFd);
  
  return ret;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in serv_sock;
  char *basedir;
  uint16_t port;

  if (argc < 3) {
    basedir = "./";
    port = 69;
  } else {
    basedir = argv[1];
    port = atoi(argv[2]);
  }

  gBasedir = strdup(basedir);
  gPort = htons(port);

  DBG(INFO, "Serving files out of %s on %d\n", gBasedir, port);

  if (chdir(gBasedir)< 0) {
    DBG(CRITICAL, "Failed to chdir into %s\n", gBasedir);
    return 1;
  }

  if ((gSockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    DBG(CRITICAL, "Failed to create server sock\n");
    return 1;
  }

  serv_sock.sin_family = AF_INET;
  serv_sock.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_sock.sin_port = gPort;

  if (bind(gSockFd, (struct sockaddr *) &serv_sock, sizeof(serv_sock)) < 0) {
    DBG(CRITICAL, "Failed to bind\n");
    return 1;
  }

  DBG(INFO, "Server successfully bound on %d\n", port);

  signal(SIGCLD, (void *)cleanupChild);

  memset(gRequestFile, 0, sizeof(gRequestFile));

  while (1) {
    struct sockaddr_in client_sock;
    socklen_t slen = sizeof(client_sock);

    tftpMessage handshake;
    ssize_t len;

    if ((len = recvMesg(gSockFd, &handshake, &client_sock, &slen)) < 0) {
      DBG(ERROR, "Server failed to receive handshake\n");
      continue;
    }

    DBG(VERBOSE, "Got message of length %lu\n", len);

    if (len < sizeof(uint16_t)) {
      DBG(ERROR, "Receive too small of message to handshake\n");
      sendError(gSockFd, 0, "Request too small", &client_sock, slen);
      continue;
    }

    uint16_t opcode = ntohs(handshake.opcode);
    if (opcode == RRQ || opcode == WRQ) {

      if (fork() == 0) {
      	exit(handle(&handshake, len, &client_sock, slen));
      }

    } else {
      DBG(ERROR, "Received message of invalid type %d\n", opcode);
      sendError(gSockFd, 0, "Invalid message type", &client_sock, slen);
    }
  }

  close(gSockFd);
}
