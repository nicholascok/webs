#include "webs_sha1.h"
#include "webs_b64.h"
#include "webs.h"

/* adds a client to the linked list of connected
 * clients */
int webs_add_client(webs_client _cli, webs_client** _r) {
	/* if first client, set head = tail = new allocation,
	 * otherwise just add after tail. (just a normal linked list) */
	if (!__webs_global.tail) {
		__webs_global.tail = __webs_global.head = malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail)
			XERR("Failed to allocate memory!", ENOMEM);
		
		__webs_global.head->prev = 0;
	} else {
		__webs_global.tail->next = malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail->next)
			XERR("Failed to allocate memory!", ENOMEM);
		
		__webs_global.tail->next->prev = __webs_global.tail;
		__webs_global.tail = __webs_global.tail->next;
	}
	
	__webs_global.tail->client = _cli;
	__webs_global.tail->next = 0;
	
	__webs_global.num_clients++;
	
	*_r = &__webs_global.tail->client;
	
	return 0;
}

/* removes a client from the linked list of
 * connected clients (note that the node pointer
 * starts with the client so passing self is
 * suffice) */
int webs_remove_client(struct webs_client_node* _node) {
	if (_node->prev)
		_node->prev->next = _node->next;
	
	if (_node->next)
		_node->next->prev = _node->prev;
	
	__webs_global.num_clients--;
	
	free(_node);
	return 0;
}

/* parses a websocket frame and returns relevant
 * data in `_frm` - this function  returns the total
 * length of the frame parsed */
int webs_parse_frame(char* _s, struct webs_frame* _frm) {
	/* offset to payload from start of frame */
	int data_start = 2;
	
	/* read the 2-byte header field */
	WORD h = CAST(_s, WORD);
	_frm->info = h;
	
	/* read the length feild (may offset payload) */
	
	/* a value of 126 here says to interpret the next two bytes */
	if (WEBSFR_GET_LENGTH(h) == 126)
		_frm->length = (WORD) BIG_ENDIAN_WORD(CAST(_s + 2, WORD)),
		data_start = 4;
	
	/* a value of 127 says to interpret the next eight bytes */
	else
	if (WEBSFR_GET_LENGTH(h) == 127)
		_frm->length = (QWORD) BIG_ENDIAN_QWORD(CAST(_s + 2, QWORD)),
		data_start = 10;
	
	/* otherwise, the raw value is used */
	else
		_frm->length = WEBSFR_GET_LENGTH(h);
	
	/* if the data is masked, the payload is further offset
	 * to fit a four byte key */
	if (WEBSFR_GET_MASKED(h))
		_frm->key = CAST(_s + data_start, DWORD),
		data_start += 4;
	
	/* set pointer to payload based on calculated offset */
	_frm->data = _s + data_start;
	_frm->off = data_start;
	
	return data_start + _frm->length;
}

/* generates a websocket fame from the provided data */
int webs_generate_frame(char* _src, char* _dst, size_t _n) {
	int data_start = 2;
	WORD hdr = 0;
	
	WEBSFR_SET_FINISH(hdr, 0x1);
	WEBSFR_SET_OPCODE(hdr, 0x1);
	
	if (_n > 125)
		WEBSFR_SET_LENGTH(hdr, 126),
		CAST(_dst + 2, WORD) = BIG_ENDIAN_WORD((WORD) _n),
		data_start = 4;
	
	else
	if (_n > 65536)
		WEBSFR_SET_LENGTH(hdr, 127),
		CAST(_dst + 2, QWORD) = BIG_ENDIAN_WORD((QWORD) _n),
		data_start = 10;
	
	else
		WEBSFR_SET_LENGTH(hdr, _n),
	
	CAST(_dst, WORD) = hdr;
	
	memcpy(_dst + data_start, _src, _n);
	
	return _n + data_start;
}

/* decodes XOR encrypted data from a websocket frame */
int webs_decode_data(char* _dta, DWORD _key, size_t _n) {
	size_t i;
	
	for (i = 0; i < _n; i++)
		_dta[i] ^= ((char*) &_key)[i % 4];
	
	return 0;
}

/* parses an HTTP header for web-socket related data:
 * returns -1 if request could not be processed, and
 * otherwise 0, storing the result in `_rtn` */
int webs_process_handshake(char* _src, struct webs_info* _rtn) {
	BYTE http_vrs_low = 0;
	
	char param_str[256];
	char req_type[8];
	int nbytes = 0;
	
	sscanf(_src, "%s %*s HTTP/%c.%c%*[^\r]\r%n", req_type, (char*) &_rtn->http_vrs, &http_vrs_low, &nbytes);
	_src += nbytes;
	
	if (!str_cmp(req_type, "GET"))
		return -1;
	
	_rtn->http_vrs <<= 8;
	_rtn->http_vrs += http_vrs_low;
	
	while (sscanf(_src, "%s%n", param_str, &nbytes) > 0) {
		_src += nbytes;
		
		if (str_cmp(param_str, "Sec-WebSocket-Version:")) {
			sscanf(_src, "%hu%*[^\r]\r%n", &_rtn->webs_vrs, &nbytes);
			_src += nbytes;
		}
		
		else
		if (str_cmp(param_str, "Sec-WebSocket-Key:")) {
			sscanf(_src, "%s%*[^\r]\r%n", _rtn->webs_key, &nbytes);
			_src += nbytes;
		}
		
		else {
			sscanf(_src, "%*[^\r]\r%n", &nbytes);
			_src += nbytes;
		}
	}
	
	if (!(_rtn->webs_key[0] || _rtn->webs_vrs || _rtn->http_vrs))
		return -1;
	
	return 0;
}

/* generates an HTTP handshake response for a
 * client requesting websocket. */
int webs_generate_handshake(char* _d, char* _key) {
	char buf[61 + 19]; /* 61 needed, extra for safety */
	char hash[21];
	int len = 0;
	
	len = str_cat(buf, _key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	webs_sha1(buf, hash, len);
	len = webs_b64_encode(hash, buf, 20);
	buf[len] = '\0';
	
	return sprintf(_d, WEBS_RESPONSE_FMT, buf);
}

/* user functions used to send data over a websocket */
int webs_send(webs_client* _self, char* _data) {
	int len = 0;
	
	/* check for nullptr or empty string */
	if (!_data || !*_data) return 0;
	
	/* get length of data */
	while (_data[++len]);
	
	/* write data */
	return write(
		_self->fd,
		_self->buf_send.data,
		webs_generate_frame(_data, _self->buf_send.data, len)
	);
	
	return 0;
}

int webs_sendn(webs_client* _self, char* _data, size_t _n) {
	return write(
		_self->fd,
		_self->buf_send.data,
		webs_generate_frame(_data, _self->buf_send.data, _n)
	);
}

/* binds the socket `_soc` to address `_addr` (a string)
 * and port `_port` (16-bit integer), returns 0 on
 * success, or a negative error code otherwise */
int bind_address(int _soc, int16_t _port) {
	struct sockaddr_in soc_addr;
	int error;
	
	/* init struct */
	soc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	soc_addr.sin_port = htons(_port);
	soc_addr.sin_family = AF_INET;
	
	/* bind socket */
	error = bind(_soc, (struct sockaddr*) &soc_addr, sizeof(soc_addr));
	return -(error < 0);
}

/* accepts a connection from a client, and gives the
 * client its data, returns the client fd on success,
 * and a negatvie error code otherwise */
int accept_connection(int _soc, webs_client* _c) {
	static long client_id_counter = 0;
	socklen_t addr_size = sizeof(_c->addr);
	
	_c->fd = accept(_soc, (struct sockaddr*) &_c->addr, &addr_size);
	if (_c->fd < 0) return -1;
	
	_c->id = client_id_counter;
	client_id_counter++;
	
	return _c->fd;
}

/* user function to close a client socket */
int webs_eject(webs_client* _self) {
	/* call client on_close function */
	(*WEBS_EVENTS.on_close)(_self);
	
	close(_self->fd);
	pthread_cancel(_self->thread);
	webs_remove_client((struct webs_client_node*) _self);
	
	return 0;
}

/* shuter down */
int webs_close(void) {
	struct webs_client_node* node = __webs_global.head;
	struct webs_client_node* temp;
	
	pthread_cancel(__webs_global.thread);
	close(__webs_global.soc);
	
	while (node) {
		temp = node->next;
		webs_eject(&node->client);
		node = temp;
	}
	
	return 0;
}

/* main client function, called on a thread for each
 * connected client */
int __webs_client_main(webs_client* _self) {
	const WORD PING = 0x008A;
	
	int cont = 0; /* used for handleing continuation frames */
	size_t fr_size = 0;
	size_t off = 0;
	
	struct webs_info ws_info = {0};
	struct webs_frame frm;
	
	char* data; /* used to store data passed to on_data function */
	
	/* wait for HTTP websocket request header */
	_self->buf_recv.len = read(_self->fd, _self->buf_recv.data, PACKET_MAX - 1);
	
	if (_self->buf_recv.len == SIZE_MAX) {
		(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_NO_HANDSHAKE);
		close(_self->fd);
		webs_remove_client((struct webs_client_node*) _self);
		return 0;
	}
	
	/* process handshake */
	_self->buf_recv.data[_self->buf_recv.len] = '\0';
	
	if (webs_process_handshake(_self->buf_recv.data, &ws_info) < 0) {
		(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_BAD_REQUEST);
		close(_self->fd);
		webs_remove_client((struct webs_client_node*) _self);
		return 0;
	}
	
	/* generate + tansmit response */
	_self->buf_send.len = webs_generate_handshake(_self->buf_send.data, ws_info.webs_key);
	send(_self->fd, _self->buf_send.data, _self->buf_send.len, 0);
	
	/* call client on_open function */
	(*WEBS_EVENTS.on_open)(_self);
	
	/* main loop */
	for (;;) {
		/* recieve data */
		if ((_self->buf_recv.len = read(_self->fd, _self->buf_recv.data, PACKET_MAX - 1)) < 1) break;
		
		/* parse data from frame */
		fr_size += webs_parse_frame(_self->buf_recv.data, &frm);
		
		CONTINUE:
		
		/* respond to pings */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x9) {
			webs_sendn(_self, (char*) &PING, 2);
			continue;
		}
		
		/* only accept text/binary/continuation frames */
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0
		 && WEBSFR_GET_OPCODE(frm.info) != 0x1
		 && WEBSFR_GET_OPCODE(frm.info) != 0x2) continue;
		
		/* handle continuation frames */
		if (!WEBSFR_GET_FINISH(frm.info)) {
			if (WEBSFR_GET_OPCODE(frm.info) != 0x0) {
				data = malloc(frm.length + 1);
				
				if (!data)
					XERR("Failed to allocate memory!", ENOMEM);
				
				memcpy(data, frm.data, frm.length);
				cont = 1, off = 0;
			}
			
			else {
				data = realloc(data, off + frm.length + 1);
				
				if (!data)
					XERR("Failed to allocate memory!", ENOMEM);
				
				memcpy(data + off, frm.data, frm.length);
			}
			
			webs_decode_data(data + off, frm.key, frm.length);
			off += frm.length;
			
			continue;
		}
		
		/* deal with non-fragmented frames */
		if (!cont) {
			data = malloc(frm.length + 1);
			
			if (!data)
				XERR("Failed to allocate memory!", ENOMEM);
			
			memcpy(data, frm.data, frm.length);
			webs_decode_data(data, frm.key, frm.length);
			off = frm.length;
		}
		
		else cont = 0;
		
		/* call clinet on_data function */
		data[off] = '\0';
		if (data) (*WEBS_EVENTS.on_data)(_self, data, off);
		free(data);
		
		/* if buffer picked up multiple frames in one read,
		 * continue parsing until we have exhausted them */
		if (fr_size < _self->buf_recv.len) {
			fr_size += webs_parse_frame(_self->buf_recv.data + fr_size, &frm);
			goto CONTINUE;
		}
		
		fr_size = 0;
		continue;
	}
	
	/* call client on_close function */
	(*WEBS_EVENTS.on_close)(_self);
	
	close(_self->fd);
	webs_remove_client((struct webs_client_node*) _self);
	
	return 0;
}

/* blocks until webs closes */
int webs_hold(void) {
	return pthread_join(__webs_global.thread, 0);
}

/* main loop, listens for connectinos and forks
 * them off for further initialisation */
int __webs_main() {
	webs_client* user_ptr;
	webs_client user;
	
	for (;;) {
		user.fd = accept_connection(__webs_global.soc, &user);
		
		if (user.fd >= 0) {
			if (webs_add_client(user, &user_ptr) < 0) continue;
			pthread_create(&user_ptr->thread, 0, (void*(*)(void*)) __webs_client_main, user_ptr);
		}
	}
	
	return 0;
}

/* initialises a socket and starts listening for
 * connections: returns a negative error code on
 * error, otherwise 0 */
int webs_start(int _port) {
	const int ONE = 1;
	int error = 0;
	
	/* basic socket setup */
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc < 0) return error;
	
	/* allow reconnection to socket (for sanity) */
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(int));
	
	error = bind_address(soc, _port);
	if (error < 0) return error;
	
	error = listen(soc, WEBS_SOCK_BACKLOG_MAX);
	if (error < 0) return error;
	
	__webs_global.soc = soc;
	
	/* fork further processing to seperate thread */
	pthread_create(&__webs_global.thread, 0, (void*(*)(void*)) __webs_main, 0);
	
	return 0;
}
