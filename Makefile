CC=gcc
# -std=c99
CFLAGS=-DDEBUG -DLOG_USE_COLOR -I. -O2 -W -Werror -Wall
PROG=camel
OBJS=message.o log.o

all: clean $(PROG) httpstub client

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(PROG): $(OBJS)
	$(CC) camel.c $(OBJS) -o $(PROG) $(CFLAGS)

client: $(OBJS)
	$(CC) client.c $(OBJS) -o client $(CFLAGS)

httpstub:
	$(CC) httpstub.c -o httpstub -lpthread

.PHONY:clean
clean:
	rm -rf *.o $(PROG) httpstub client
