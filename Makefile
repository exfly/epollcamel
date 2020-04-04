CC=gcc
CFLAGS=-DDEBUG -I. -O2 -W -Werror -Wall
PROG=camel
OBJS=message.o 

all: clean $(PROG) httpstub client

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(PROG): $(OBJS)
	$(CC) camel.c $(OBJS) -o $(PROG) -lpthread

client: $(OBJS)
	$(CC) client.c $(OBJS) -o client -lpthread

httpstub:
	$(CC) httpstub.c -o httpstub -lpthread

.PHONY:clean
clean:
	rm -rf *.o $(PROG) httpstub client
