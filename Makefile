CC=gcc
CFLAGS=-O2 -W -Werror -Wall
PROG=camel
OBJS=camel.o message.o 

all: $(PROG) httpstub

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(PROG): $(OBJS)
	$(CC) $(OBJS) -o $(PROG) -lpthread

httpstub:
	$(CC) httpstub.c -o httpstub -lpthread

.PHONY:clean

clean:
	rm -rf *.o $(PROG) httpstub
