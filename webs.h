#ifndef __WEBS_H__
#define __WEBS_H__

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 
 * macros to report runtime errors...
 */
#define __WEBS_XE_PASTE_WRAPPER(x) __WEBS_XE_PASTE_RAW(x)
#define __WEBS_XE_PASTE_RAW(x) #x
#define __WEBS_XE_PASTE(V) __WEBS_XE_PASTE_WRAPPER(V)

#if __STDC_VERSION__ > 199409L
	#ifdef NOESCAPE
		#define WEBS_XERR(MESG, ERR) { printf("Runtime Error: (in "__WEBS_XE_PASTE(__FILE__)", func: %s [line "__WEBS_XE_PASTE(__LINE__)"]) : "MESG"\n", __func__); exit(ERR); }
	#else
		#define WEBS_XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mRuntime Error: \x1b[0m(in "__WEBS_XE_PASTE(__FILE__)", func: \x1b[1m%s\x1b[0m [line \x1b[1m"__WEBS_XE_PASTE(__LINE__)"\x1b[0m]) : "MESG"\n", __func__); exit(ERR); }
	#endif
#else
	#ifdef NOESCAPE
		#define WEBS_XERR(MESG, ERR) { printf("Runtime Error: (in "__WEBS_XE_PASTE(__FILE__)", line "__WEBS_XE_PASTE(__LINE__)") : "MESG"\n"); exit(ERR); }
	#else
		#define WEBS_XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mRuntime Error: \x1b[0m(in "__WEBS_XE_PASTE(__FILE__)", line \x1b[1m"__WEBS_XE_PASTE(__LINE__)"\x1b[0m) : "MESG"\n"); exit(ERR); }
	#endif
#endif


/* 
 * declare endian-independant macros
 */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	
	#define WEBS_BIG_ENDIAN_WORD(X) X
	
	#define WEBS_BIG_ENDIAN_DWORD(X) X
	
	#define WEBS_BIG_ENDIAN_QWORD(X) X
	
#else
	
	#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
		#warning could not determine system endianness (assumng little endian).
	#endif
	
	#define WEBS_BIG_ENDIAN_WORD(X) (((X << 8) & 0xFF00) | ((X >> 8) & 0x00FF))
	
	#define WEBS_BIG_ENDIAN_DWORD(X) ((uint32_t) (\
		(((uint32_t) X >> 24) & 0x000000FFUL) |\
		(((uint32_t) X >> 8 ) & 0x0000FF00UL) |\
		(((uint32_t) X << 8 ) & 0x00FF0000UL) |\
		(((uint32_t) X << 24) & 0xFF000000UL)))
	
	#define WEBS_BIG_ENDIAN_QWORD(X) ( __WEBS_BIG_ENDIAN_QWORD(X) )
	
#endif

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
#define WEBSFR_GET_LENGTH(H) ( ((uint8_t*) &H)[1] & WEBSFR_LENGTH_MASK[1] )
#define WEBSFR_GET_OPCODE(H) ( ((uint8_t*) &H)[0] & WEBSFR_OPCODE_MASK[0] )
#define WEBSFR_GET_MASKED(H) ( ((uint8_t*) &H)[1] & WEBSFR_MASKED_MASK[1] )
#define WEBSFR_GET_FINISH(H) ( ((uint8_t*) &H)[0] & WEBSFR_FINISH_MASK[0] )
#define WEBSFR_GET_RESVRD(H) ( ((uint8_t*) &H)[0] & WEBSFR_RESVRD_MASK[0] )

#define WEBSFR_SET_LENGTH(H, V) ( ((uint8_t*) &H)[1] |= V & 0x7F )
#define WEBSFR_SET_OPCODE(H, V) ( ((uint8_t*) &H)[0] |= V & 0x0F )
#define WEBSFR_SET_MASKED(H, V) ( ((uint8_t*) &H)[1] |= (V << 7) & 0x80 )
#define WEBSFR_SET_FINISH(H, V) ( ((uint8_t*) &H)[0] |= (V << 7) & 0x80 )
#define WEBSFR_SET_RESVRD(H, V) ( ((uint8_t*) &H)[0] |= (V << 4) & 0x70 )

/* 
 * HTTP response format for confirming a websocket connection.
 */
#define WEBS_RESPONSE_FMT "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

/* 
 * macro to convert an integer to its base-64 representation.
 */
#define TO_B64(X) ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[X])

/* 
 * cast macro (can be otherwise pretty ugly)
 */
#define CASTP(X, T) (*((T*) (X)))

/* 
 * bitwise rotate left
 */
#define ROL(X, N) ((X << N) | (X >> ((sizeof(X) * 8) - N)))

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

/**
 * checks a client out of the server to which it is connected.
 * @param _self: the client to be ejected.
 * @note for user functions, passing self (a webs_client pointer) is suffice.
 */
void webs_eject(webs_client* _self);

/**
 * closes a websocket server.
 * @param _srv: the server that is to be shut down.
 */
void webs_close(webs_server* _srv);

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

/* 
 * C89 doesn't officially support 64-bt integer constants, so
 * thats why this mess is here...  (there is a better way)
 */
uint64_t __WEBS_BIG_ENDIAN_QWORD(uint64_t _x);

#endif /* __WEBS_H__ */
