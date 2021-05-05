#ifndef _ZTFTPD_H_
#define _ZTFTPD_H_

#define DEFAULT_WINDOW_SIZE 1
#define DEFAULT_BLK_SIZE 512
/* reverse:  reverse string s in place */
void reverse(char s[])
{
  int i, j;
  char c;

  for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

void itoa(int n, char s[])
{
  int i, sign;

  if ((sign = n) < 0)  /* record sign */
    n = -n;          /* make n positive */
  i = 0;
  do {       /* generate digits in reverse order */
    s[i++] = n % 10 + '0';   /* get next digit */
  } while ((n /= 10) > 0);     /* delete it */
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  reverse(s);
}

/* tftp opcode mnemonic */
enum opcode {
     RRQ=1,
     WRQ,
     DATA,
     ACK,
     ERROR,
     OACK
};

/* tftp transfer mode */
enum mode {
     NETASCII=1,
     OCTET
};

typedef struct {

  uint16_t opcode;

  union {
    struct {
      uint8_t filename[514];
    } request;     

    struct {
      uint16_t block;
      uint8_t data[512];
    } data;

    struct {
      uint16_t block;
    } ack;

    struct {
      uint16_t code;
      uint8_t  string[512];
    } error;

    struct {
      uint8_t options[514];
    } oack;

  } contents;

} tftpMessage;


#endif
