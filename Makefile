

CFLAGS = -Wall -g -std=c11 -D _POSIX_C_SOURCE=200809L -Werror

all: build

build:
	gcc $(CFLAGS) s-talk.c instructorList.o -lpthread -o s-talk


run: 
	./s-talk

valgrind: build
	valgrind --leak-check=full ./s-talk

clean:
	rm -f s-talk
