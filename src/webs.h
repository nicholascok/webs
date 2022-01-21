#ifndef __WEBS_H__
#define __WEBS_H__

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "webs_endian.h"
#include "webs_string.h"
#include "webs_sha1.h"
#include "webs_b64.h"
#include "error.h"

/* 
 * make sure SSIZE_MAX is defined.
 */
#ifndef SSIZE_MAX
	#define SSIZE_MAX (((size_t) (~0)) >> 1)
#endif

/* 
 * buffer sizes...
 */
#define WEBS_MAX_PACKET 32768
#define WEBS_MAX_BACKLOG 8

/* 
 * maximum packet recieve size is SSIZE_MAX.
 */
#define WEBS_MAX_RECIEVE SSIZE_MAX

/* 
 * macros for extracting bitwise data from a websocket frame's 16-bit
 * header.
 */
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

/* 
 * HTTP response format for confirming a websocket connection.
 */
#define WEBS_RESPONSE_FMT "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

/* 
 * cast macro (can be otherwise pretty ugly)
 */
#define CASTP(X, T) (*((T*) (X)))

typedef struct webs_server webs_server;
typedef struct webs_client webs_client;

/* 
 * list of errors passed to `on_error`
 */
enum webs_error {
	WEBS_ERR_NONE = 0,
	WEBS_ERR_READ_FAILED,
	WEBS_ERR_UNEXPECTED_CONTINUTATION,
	WEBS_ERR_NO_SUPPORT,
	WEBS_ERR_OVERFLOW
};

/* 
 * declare masks in an endian-independant way.
 */
extern uint8_t WEBSFR_LENGTH_MASK[2];
extern uint8_t WEBSFR_OPCODE_MASK[2];
extern uint8_t WEBSFR_MASKED_MASK[2];
extern uint8_t WEBSFR_FINISH_MASK[2];
extern uint8_t WEBSFR_RESVRD_MASK[2];

/* headers for sending ping / pong requests, as above.
 */
extern uint8_t WEBS_PING[2];
extern uint8_t WEBS_PONG[2];

/* 
 * stores header data from a websocket frame.
 */
struct webs_frame {
	ssize_t length; /* length of the frame's payload in bytes */
	uint32_t key;   /* a 32-bit key used to decrypt the frame's
	                 *   payload (provided per frame) */
	uint16_t info;  /* the 16-bit frame header */
	short off;      /* offset from star of frame to payload*/
};

/* 
 * stores data parsed from an HTTP websocket request.
 */
struct webs_info {
	char webs_key[24 + 1]; /* websocket key (base-64 encoded string) */
	uint16_t webs_vrs;     /* websocket version (integer) */
	uint16_t http_vrs;     /* HTTP version (concatonated chars) */
};

/* 
 * used for sending / receiving data.
 */
struct webs_buffer {
	char data[WEBS_MAX_PACKET];
	ssize_t len;
};

/* 
 * user-implemented event handlers.
 */
struct webs_event_list {
	int (*on_error)(struct webs_client*, enum webs_error);
	int (*on_data )(struct webs_client*, char*, ssize_t);
	int (*on_open )(struct webs_client*);
	int (*on_close)(struct webs_client*);
	int (*on_pong)(struct webs_client*);
	int (*on_ping)(struct webs_client*);
};

/* 
 * holds information relevant to a client.
 */
struct webs_client {
	struct webs_server* srv; /* a pointer to the server the the
	                          *   clinet is connected to */
	struct sockaddr_in addr; /* client address */
	pthread_t thread;        /* client's posix thread id */
	size_t id;               /* client's internal id */
	int fd;                  /* client's descriptor */
};

/* 
 * holds information relevant to a server.
 */
struct webs_server {
	struct webs_event_list events;
	struct webs_client_node* head;
	struct webs_client_node* tail;
	size_t num_clients;
	pthread_t thread;
	size_t id;
	int soc;
};

/* 
 * element in a linked list of connected clients.
 */
struct webs_client_node {
	struct webs_client client;
	struct webs_client_node* next;
	struct webs_client_node* prev;
};

/* default handlers for client events */
int webs_default_handler0(struct webs_client* _self);
int webs_default_handler1(struct webs_client* _self, char* _data, ssize_t _n);
int webs_default_handler2(struct webs_client* _self, enum webs_error _ec);
int webs_default_handlerP(struct webs_client* _self);

/**
 * removes a client from a server's internal listing.
 * @param _node: a pointer to the client in the server's listing.
 * @note for user functions, passing self (a webs_client pointer) is suffice.
 */
void webs_remove_client(struct webs_client_node* _node);

/**
 * checks a client out of the server to which it is connected.
 * @param _self: the client to be ejected.
 */
void webs_eject(webs_client* _self);

/**
 * closes a websocket server.
 * @param _srv: the server that is to be shut down.
 */
void webs_close(webs_server* _srv);

/**
 * wraper functon that deals with reading lage amounts
 * of data, as well as attemts to complete partial reads.
 * @param _fd: the file desciptor to be read from.
 * @param _dst: a buffer to store the resulting data.
 * @param _n: the number of bytes to be read.
 */
ssize_t webs_asserted_read(int _fd, void* _dst, size_t _n);

/**
 * decodes XOR encrypted data from a websocket frame.
 * @param _dta: a pointer to the data that is to be decrypted.
 * @param _key: a 32-bit key used to decrypt the data.
 * @param _n: the number of bytes of data to be decrypted.
 */
int webs_decode_data(char* _dta, uint32_t _key, ssize_t _n);

/**
 * user function used to send null-terminated data over a
 * websocket.
 * @param _self: the client who is sending the data.
 * @param _data: a pointer to the null-terminated data
 * that is to be sent.
 * @return the result of the write.
 */
int webs_send(webs_client* _self, char* _data);

/**
 * user function used to send binary data over a websocket.
 * @param _self: the client who is sending the data.
 * @param _data: a pointer to the data to is to be sent.
 * @param _n: the number of bytes that are to be sent.
 * @return the result of the write.
 */
int webs_sendn(webs_client* _self, char* _data, ssize_t _n);

/**
 * sends a pong frame to a client over a websocket.
 * @param _self: the client that the pong is to be sent to.
 */
void webs_pong(webs_client* _self);

/**
 * blocks until a server's thread closes (likely the
 * server has been closed with a call to "webs_close()").
 * @param _srv: the server that is to be waited for.
 * @return the result of pthread_join(), or -1 if NULL
 * was provided.
 */
int webs_hold(webs_server* _srv);

/**
 * initialises a websocket sever and starts listening for
 * connections.
 * @param _port: the port to listen on.
 * @return 0 if the server could not be created, or a pointer
 * to the newly created server otherwise.
 */
webs_server* webs_start(int _port);

#endif /* __WEBS_H__ */
