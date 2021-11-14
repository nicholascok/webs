all:
	gcc -o test ./examples/test.c -lpthread -Wall -Wextra -Wpedantic -std=c90 -Wno-cast-function-type -Wno-unused-variable -Wno-unused-parameter 
