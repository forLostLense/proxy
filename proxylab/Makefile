TEAM = NOBODY
VERSION = 1

CC = gcc
CFLAGS = -Wall -g -pthread
LDFLAGS = -g -pthread

OBJS = proxy.o csapp.o strmanip.o

all: proxy

proxy: $(OBJS)

proxy.o csapp.o: csapp.h
proxy.o strmanip.o: strmanip.h

handin:
	cs105submit proxy.c

clean:
	rm -f *~ *.o proxy core

