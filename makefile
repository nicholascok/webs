CFLAGS := -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-cast-function-type
STD := c89
CC := gcc

all: compile build

compile:
	@echo "build options:"
	@echo "CFLAGS = ${CFLAGS}"
	@echo "STD    = ${STD}"
	@echo "CC     = ${CC}"
	@echo
	
	$(CC) -c src/*.c examples/test.c $(CFLAGS) -std=$(STD)

build: compile
	$(CC) -o webs *.o -lpthread

clean:
	-rm -f webs
	-rm -f *.o
