CC = gcc

all: camel client epoll_demo

message.o: message.h
	$(CC) message.c -c -o message.o -Werror -g

camel:
	$(CC) camel.c -o camel -Werror -g

epoll_demo:
	$(CC) epoll_demo.c -o epoll_demo -Werror -g

client: message.o
	$(CC) client.c -o client -Werror -g message.o

clean:
	@rm -v epoll_demo camel client message.o
