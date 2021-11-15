#ifndef __WEBS_H__
#define __WEBS_H__

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>

/* check for POSIX compliance (no support for windows yet) */
#ifndef _POSIX_VERSION
	#error CRD_WEBS requires a POSIX compliant system
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "webs_endian.h"
#include "error.h"

#define WEBS_MAX_PACKET 32768

/* max packet recieve size is ssize_t max */
#define WEBS_MAX_RECIEVE (((ssize_t) (~0UL)) * -1)
#define WEBS_MAX_BACKLOG 8

/* declar headers for sending ping/pong requests */
const uint8_t PING[2];
const uint8_t PONG[2];

/* declare masks in an endian-independant way
 * (defined in "webs.c") */
const uint8_t WEBSFR_LENGTH_MASK[2];
const uint8_t WEBSFR_OPCODE_MASK[2];
const uint8_t WEBSFR_MASKED_MASK[2];
const uint8_t WEBSFR_FINISH_MASK[2];
const uint8_t WEBSFR_RESVRD_MASK[2];

/* macros for extracting bitwise data from a websocket frame's 16-bit header */
#define WEBSFR_GET_LENGTH(H) ((H & *((uint16_t*) &WEBSFR_LENGTH_MASK)) >> 8 )
#define WEBSFR_GET_OPCODE(H) ((H & *((uint16_t*) &WEBSFR_OPCODE_MASK)) >> 0 )
#define WEBSFR_GET_MASKED(H) ((H & *((uint16_t*) &WEBSFR_MASKED_MASK)) >> 15)
#define WEBSFR_GET_FINISH(H) ((H & *((uint16_t*) &WEBSFR_FINISH_MASK)) >> 7 )
#define WEBSFR_GET_RESVRD(H) ((H & *((uint16_t*) &WEBSFR_RESVRD_MASK)) >> 4 )

#define WEBSFR_SET_LENGTH(H, V) (H |= ((uint16_t) ((uint8_t) V & 0x7F) << 8 ))
#define WEBSFR_SET_OPCODE(H, V) (H |= ((uint16_t) ((uint8_t) V & 0x0F) << 0 ))
#define WEBSFR_SET_MASKED(H, V) (H |= ((uint16_t) ((uint8_t) V & 0x01) << 15))
#define WEBSFR_SET_FINISH(H, V) (H |= ((uint16_t) ((uint8_t) V & 0x01) << 7 ))
#define WEBSFR_SET_RESVRD(H, V) (H |= ((uint16_t) ((uint8_t) V & 0x07) << 4 ))

#define WEBS_RESPONSE_FMT "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

#define CAST(X, T) (*((T*) (X)))

typedef unsigned char webs_flag_t;

/* stores header data from a
 * websocket frame */
struct webs_frame {
	ssize_t length; /* length of the frame's payload in bytes */
	uint32_t key;   /* a 32-bit key used to decrypt the frame's
	                 * payload (provided per frame) */
	uint16_t info;  /* the 16-bit frame header */
	short off;      /* offset from star of frame to payload*/
};

/* stores data parsed from an HTTP
 * websocket request */
struct webs_info {
	char webs_key[24 + 1]; /* websocket key (base-64 encoded string) */
	uint16_t webs_vrs;     /* websocket version (integer) */
	uint16_t http_vrs;     /* HTTP version (concatonated chars) */
};

struct webs_buffer {
	char data[WEBS_MAX_PACKET];
	ssize_t len;
};

struct webs_client {
	struct webs_server* srv; /* a pointer to the server the the
	                          * clinet is connected to */
	struct sockaddr_in addr; /* client address */
	pthread_t thread;        /* client's posix thread id */
	size_t id;               /* client's internal id */
	int fd;                  /* client's descriptor */
};

/* element in a linked list of
 * connected clients */
struct webs_client_node {
	struct webs_client client;
	struct webs_client_node* next;
	struct webs_client_node* prev;
};

/* list of errors passed to `on_error`
 * (see below) */
enum webs_error {
	WEBS_ERR_NONE = 0,
	WEBS_ERR_READ_FAILED,
	WEBS_ERR_UNEXPECTED_CONTINUTATION,
	WEBS_ERR_NO_SUPPORT
};

/* default handlers for client events */
int webs_default_handler0(struct webs_client* _self);
int webs_default_handler1(struct webs_client* _self, char* _data, ssize_t _n);
int webs_default_handler2(struct webs_client* _self, enum webs_error _ec);

/* user implemented event handlers */
struct webs_event_list {
	int (*on_error)(struct webs_client*, enum webs_error);
	int (*on_data )(struct webs_client*, char*, ssize_t);
	int (*on_open )(struct webs_client*);
	int (*on_close)(struct webs_client*);
	int (*on_ping)(struct webs_client*);
	int (*on_pong)(struct webs_client*);
};

struct webs_server {
	struct webs_event_list events;
	struct webs_client_node* head;
	struct webs_client_node* tail;
	size_t num_clients;
	pthread_t thread;
	int soc;
};

typedef struct webs_server webs_server;
typedef struct webs_client webs_client;

/**
 * adds a client to a server's internal listing.
 * @param _srv: the server that the client should be added to.
 * @param _cli: the client to be added.
 * @return a pointer to the added client in the server's listing.
 * (or 0 if nullptr was provided) */
webs_client* webs_add_client(webs_server* _srv, webs_client _cli);

/**
 * removes a client from a server's internal listing.
 * @param _node: a pointer to the client in the server's listing.
 * @note for user functions, passing self (a webs_client pointer) is suffice. */
void webs_remove_client(struct webs_client_node* _node);

/**
 * checks a client out of the server to which it is connected.
 * @param _self: the client to be ejected. */
void webs_eject(webs_client* _self);

/**
 * closes a websocket server.
 * @param _srv: the server that is to be shut down. */
void webs_close(webs_server* _srv);

/**
 * empties bytes from a descriptor's internal buffer.
 * (this is used to skip frames that cannot be processed)
 * @param _fd: the descritor whos buffer is to be emptied.
 * @return the number of bytes successfully processed. */
int webs_flush(int _fd, ssize_t _n);

/**
 * wraper functon that deals with reading lage amounts
 * of data, as well as attemts to complete partial reads.
 * @param _fd: the file desciptor to be read from.
 * @param _dst: a buffer to store the resulting data.
 * @param _n: the number of bytes to be read. */
ssize_t webs_asserted_read(int _fd, void* _dst, size_t _n);

/**
 * parses a websocket frame by reading data sequentially from
 * a socket, storng the result in `_frm`.
 * @param _self: a pointer to the client who sent the frame.
 * @param _frm: a poiter to store the resulting frame data.
 * @return -1 if the frame could not be parsed, or 0 otherwise. */
int webs_parse_frame(webs_client* _self, struct webs_frame* _frm);

/**
 * generates a websocket frame from the provided data.
 * @param _src: a pointer to the frame's payload data.
 * @param _dst: a buffer that will hold the resulting frame.
 * @note the caller ensures this buffer is of adequate
 * length (it shouldn't need more than _n + 10 bytes).
 * @param _n: the size of the frame's payload data.
 * @param _op: the frame's opcode.
 * @return the total number of resulting bytes copied. */
int webs_generate_frame(char* _src, char* _dst, ssize_t _n, uint8_t _op);

/**
 * decodes XOR encrypted data from a websocket frame.
 * @param _dta: a pointer to the data that is to be decrypted.
 * @param _key: a 32-bit key used to decrypt the data.
 * @param _n: the number of bytes of data to be decrypted. */
int webs_decode_data(char* _dta, uint32_t _key, ssize_t _n);

/**
 * parses an HTTP header for web-socket related data.
 * @note this function is a bit of a mess...
 * @param _src: a pointer to the raw header data.
 * @param _rtn: a pointer to store the resulting data.
 * @return -1 on error (bad vers., ill-formed, etc.), or 0
 * otherwise. */
int webs_process_handshake(char* _src, struct webs_info* _rtn);

/**
 * generates an HTTP websocket handshake response. by the
 * specification (RFC-6455), this is done by concatonating the
 * client provided key with a magic string, and returning the
 * base-64 encoded, SHA-1 hash of the result in the "Sec-WebSocket-
 * Accept" field of an HTTP response header.
 * @param dst: a buffer that will hold the resulting HTTP
 * response data.
 * @param _key: a pointer to the websocket key provided by the
 * client in it's HTTP websocket request header.
 * @return the total number of resulting bytes copied. */
int webs_generate_handshake(char* _dst, char* _key);

/**
 * user function used to send null-terminated data over a
 * websocket.
 * @param _self: the client who is sending the data.
 * @param _data: a pointer to the null-terminated data
 * that is to be sent.
 * @return the result of the write. */
int webs_send(webs_client* _self, char* _data);

/**
 * user function used to send binary data over a websocket.
 * @param _self: the client who is sending the data.
 * @param _data: a pointer to the data to is to be sent.
 * @param _n: the number of bytes that are to be sent.
 * @return the result of the write. */
int webs_sendn(webs_client* _self, char* _data, ssize_t _n);

/**
 * binds a socket to an address and port.
 * @param _soc: the socket to be bound.
 * @param _addr: a null-terminatng string containing the
 * address that the socket should be bound to.
 * @param _port: the port that the socket should be bound
 * to as a 16-bit integer.
 * @return -1 on error, or 0 otherwise. */
int bind_address(int _soc, int16_t _port);

/**
 * accepts a connection from a client and provides it with
 * relevant data.
 * @param _soc: the socket that the connection is being requested on.
 * @param _cli: the client that is to be connected.
 * @return -1 on error, or 0 otherwise. */
int accept_connection(int _soc, webs_client* _c);

/**
 * main client function, called on a thread for each
 * connected client.
 * @param _self: the client who is calling. */
void __webs_client_main(webs_client* _self);

/**
 * blocks until a server's thread closes (likely the
 * server has been closed with a call to "webs_close()").
 * @param _srv: the server that is to be waited for.
 * @return the result of pthread_join(), or -1 if nullptr
 * was provided. */
int webs_hold(webs_server* _srv);

/**
 * main loop for a server, listens for connectinos and forks
 * them off for further initialisation.
 * @param _sv: te server that is calling. */
void __webs_main(webs_server* _srv);

/**
 * initialises a websocket sever and starts listening for
 * connections.
 * @param _port: the port to listen on.
 * @return 0 if the server could not be created, or a pointer
 * to the newly created server otherwise. */
webs_server* webs_start(int _port);

#endif