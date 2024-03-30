CC = cc
CFLAGS = -g -Wall -Os
LDFLAGS =

SRC = simplerotate.c
OBJS = $(SRC:.c=.o)

all: simplerotate

clean:
	rm -fr simplerotate *.o

simplerotate: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

depend:
	$(CC) $(CFLAGS) -MM $(SRC) > .makeinclude

include .makeinclude
