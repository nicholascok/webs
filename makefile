all:
	gcc -o test ./examples/test.c -lpthread -lcrypto -lssl
