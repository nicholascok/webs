#ifndef __WEBS_H__
#define __WEBS_H__

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include "webs_endian.h"
#include "error.h"

/* typedefs */
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;

#if !defined(_SIZE_T) && !defined(_SIZE_T_DEFINED) && !defined(_SIZE_T_DECLARED)
	#define _SIZE_T
	#define _SIZE_T_DEFINED
	#define _SIZE_T_DECLARED
	typedef unsigned long size_t;
#endif

#define PACKET_MAX 32768 + 10
#define WEBS_SOCK_BACKLOG_MAX 8

#define CAST(X, T) (*((T*) (X)))

/* declare masks in an endian-independant way */
const uint8_t WEBSFR_LENGTH_MASK[2] = {0x00, 0x7F};
const uint8_t WEBSFR_OPCODE_MASK[2] = {0x0F, 0x00};
const uint8_t WEBSFR_MASKED_MASK[2] = {0x00, 0x80};
const uint8_t WEBSFR_FINISH_MASK[2] = {0x80, 0x00};
const uint8_t WEBSFR_RESVRD_MASK[2] = {0x70, 0x00};

#define WEBSFR_GET_LENGTH(H) ((H & *((uint16_t*) &WEBSFR_LENGTH_MASK)) >> 8 )
#define WEBSFR_GET_OPCODE(H) ((H & *((uint16_t*) &WEBSFR_OPCODE_MASK)) >> 0 )
#define WEBSFR_GET_MASKED(H) ((H & *((uint16_t*) &WEBSFR_MASKED_MASK)) >> 15)
#define WEBSFR_GET_FINISH(H) ((H & *((uint16_t*) &WEBSFR_FINISH_MASK)) >> 7 )
#define WEBSFR_GET_RESVRD(H) ((H & *((uint16_t*) &WEBSFR_RESVRD_MASK)) >> 4 )

#define WEBSFR_SET_LENGTH(H, V) (H |= ((WORD) ((BYTE) V & 0x7F) << 8 ))
#define WEBSFR_SET_OPCODE(H, V) (H |= ((WORD) ((BYTE) V & 0x0F) << 0 ))
#define WEBSFR_SET_MASKED(H, V) (H |= ((WORD) ((BYTE) V & 0x01) << 15))
#define WEBSFR_SET_FINISH(H, V) (H |= ((WORD) ((BYTE) V & 0x01) << 7 ))
#define WEBSFR_SET_RESVRD(H, V) (H |= ((WORD) ((BYTE) V & 0x07) << 4 ))

#define WEBS_RESPONSE_FMT "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

enum {
	WEBS_ERR_NONE = 0,
	WEBS_ERR_NO_HANDSHAKE,
	WEBS_ERR_BAD_REQUEST
};

/* stores data from a websocket
 * frame */
struct webs_frame {
	WORD info;
	size_t length;
	WORD off;
	char* data;
	DWORD key;
} __attribute__ ((__packed__));

/* stores data parsed from a
 * HTTP websocket request */
struct webs_info {
	char webs_key[24 + 1]; /* base-64 encoded string (web-socket key) */
	WORD webs_vrs;         /* web-socket version [integer] */
	WORD http_vrs;         /* http version [concatonated chars] */
};

struct webs_buffer {
	char data[PACKET_MAX];
	size_t len;
};

/* global struct that stores
 * internal information */
struct {
	struct webs_client_node* head;
	struct webs_client_node* tail;
	size_t num_clients;
	pthread_t thread;
	int soc;
} __webs_global = {0};

/* client struct: one is made
 * for each connected client -
 * this struct is to be passed to
 * event handlers as `self`. */
struct webs_client {
	struct sockaddr_in addr;
	struct webs_buffer buf_send;
	struct webs_buffer buf_recv;
	pthread_t thread;
	long id;
	int fd;
};

typedef struct webs_buffer webs_buffer;
typedef struct webs_client webs_client;

/* linked list of clients, see
 * _g_webs_clis next */
struct webs_client_node {
	struct webs_client client;
	struct webs_client_node* next;
	struct webs_client_node* prev;
};

/* functions */
int str_cat(char* _buf, char* _a, char* _b) {
	int len = 0;
	while (*_a) *(_buf++) = *(_a++), len++;
	while (*_b) *(_buf++) = *(_b++), len++;
	*_buf = '\0';
	return len;
}

int str_cmp(char* _a, char* _b) {
	int i;
	for (i = 0; _a[i] == _b[i]; i++)
		if (!_a[i] && !_b[i]) return 1;
	return 0;
}

int str_cmp_insensitive(char* _a, char* _b) {
	int i;
	for (i = 0; _a[i] == _b[i] || _a[i] == _b[i] - 0x20 || _a[i] == _b[i] + 0x20; i++)
		if (!_a[i] && !_b[i]) return 1;
	return 0;
}

/* declarations */
int webs_void_handler0(webs_client* c                   ) {return c->id;                   }
int webs_void_handler1(webs_client* c, char* d, size_t l) {return c->id + (intptr_t) d + l;}
int webs_void_handler2(webs_client* c, int e            ) {return c->id + e;               }

struct {
	int (*on_error)(webs_client*, int);
	int (*on_data)(webs_client*, char*, size_t);
	int (*on_open)(webs_client*);
	int (*on_close)(webs_client*);
} WEBS_EVENTS = {
	webs_void_handler2,
	webs_void_handler1,
	webs_void_handler0,
	webs_void_handler0,
};

#endif
