#Compiler
CC = gcc

#Compiler flags
#Release
#CFLAGS = -02 -Wall -I .

#Debug
CFLAGS = -g -Wall -Wextra -I .

#Library flags
LDFLAGS = -lpthread -lcurl

all: main.out

main.out: main.c
	$(CC) $(CFLAGS) -o main.out main.c $(LDFLAGS)

clean:
	rm -f *.o main.out *~