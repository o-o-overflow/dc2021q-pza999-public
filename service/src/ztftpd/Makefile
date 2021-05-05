CD ?= cd
MKDIR ?= mkdir
OUTDIR ?= out
CFLAGS ?= -Wall -static
SRC=$(wildcard *.c)

all: zTftpd

zTftpd: $(SRC)
	$(CC) -o $@ $^ $(CFLAGS)

