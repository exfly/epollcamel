CC = gcc

all: camel
 
camel:
	$(CC) camel.c -o camel -Werror -g

clean:
	@rm -v camel
