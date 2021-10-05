#include "webs_b64.h"
#include "webs.h"

/* adds a client to the linked list of connected
 * clients */
int webs_add_client(webs_client _cli, webs_client** _r) {
	/* if first client, set head = tail = new allocation,
	 * otherwise just add after tail. (just a normal linked list)*/
	if (!__webs_global.tail) {
		__webs_global.tail =
		__webs_global.head = malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail) {
			__webs_global.tail = __webs_global.head = 0;
			return -1;
		}
		
		__webs_global.head->prev = 0;
	} else {
		__webs_global.tail->next = malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail->next) {
			__webs_global.tail->next = 0;
			return -1;
		}
		
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
	
	free(_node);
	return 0;
}

/* parses a websocket frame and returns relevant
 * data in `_frm` */
int webs_parse_frame(char* _s, struct webs_frame* _frm) {
	WORD h = CAST(_s, WORD);
	*_frm = (struct webs_frame) {h, WEBSFR_GET_LENGTH(h), 0, 0};
	
	int data_start = 2;
	
	if (WEBSFR_GET_LENGTH(h) == 126)
		_frm->length = (WORD) FIX_ENDIAN_WORD(CAST(_s + 2, WORD)),
		data_start = 4;
	
	else
	if (WEBSFR_GET_LENGTH(h) == 127)
		return -1;
	
	if (WEBSFR_GET_MASKED(h))
		_frm->key = CAST(_s + data_start, DWORD),
		data_start += 4;
	
	_frm->data = _s + data_start;
	_frm->off = data_start;
	
	return 0;
}

int webs_generate_frame(char* _src, char* _dst, size_t _n) {
	int data_start = 2;
	WORD hdr = 0;
	
	WEBSFR_SET_FINISH(hdr, 0x1);
	WEBSFR_SET_OPCODE(hdr, 0x1);
	
	if (_n > 125)
		WEBSFR_SET_LENGTH(hdr, 126),
		CAST(_dst + 2, WORD) = FIX_ENDIAN_WORD((WORD) _n),
		data_start = 4;
	
	else
	if (_n > 65536)
		WEBSFR_SET_LENGTH(hdr, 127),
		CAST(_dst + 2, QWORD) = FIX_ENDIAN_WORD((QWORD) _n),
		data_start = 10;
	
	else
		WEBSFR_SET_LENGTH(hdr, _n),
	
	CAST(_dst, WORD) = hdr;
	
	memcpy(_dst + data_start, _src, _n);
	
	return _n + data_start;
}

/* decodes XOR encrypted data from a websocket client */
int webs_decode_data(BYTE* _dta, DWORD _key, size_t _n) {
	for (int i = 0; i < _n; i++)
		_dta[i] ^= ((BYTE*) &_key)[i % 4];
	
	return 0;
}

/* parses an HTTP header for web-socket related data:
 * returns -1 if request could not be processed, and
 * otherwise 0, storing the result in `_rtn` */
int webs_process_handshake(char* _src, struct webs_info* _rtn) {
	*_rtn = (struct webs_info) {0};
	WORD http_vrs_low = 0;
	
	char req_type[8];
	int nbytes = 0;
	
	sscanf(_src, "%s %*s HTTP/%c.%c%*[^\r]\r%n", &req_type, &_rtn->http_vrs, &http_vrs_low, &nbytes);
	_src += nbytes;
	
	if (!str_cmp(req_type, "GET"))
		return -1;
	
	_rtn->http_vrs <<= 8;
	_rtn->http_vrs += http_vrs_low;
	
	char param_str[256];
	
	while (sscanf(_src, "%s%n", &param_str, &nbytes) > 0) {
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
	
	if (!(_rtn->webs_key[0] || _rtn->webs_vrs || _rtn->http_vrs)) {
		*_rtn = (struct webs_info) {0};
		return -1;
	}
	
	return 0;
}

/* generates an HTTP handshake response for a
 * client requesting websocket. */
int webs_generate_handshake(char* _d, char* _key) {
	char buf[61 + 19]; // 61 needed, extra for safety
	char hash[20 + 12]; // likewise
	int len = 0;
	
	len = str_cat(buf, _key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	SHA1(buf, len, hash);
	len = b64_encode(hash, buf, 20);
	buf[len] = '\0';
	
	return sprintf(_d, WEBS_RESPONSE_FMT, buf);
}

/* user functions used to send data over a websocket */
int webs_send(webs_client* _self, char* _data) {
	if (!*_data) write(
		_self->fd,
		_self->buf_send.data,
		webs_generate_frame(_data, _self->buf_send.data, 0)
	);
	
	else {
		int len = 0;
		while (_data[++len]);
		
		return write(
			_self->fd,
			_self->buf_send.data,
			webs_generate_frame(_data, _self->buf_send.data, len)
		);
	}
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
	
	// init struct
	soc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	soc_addr.sin_port = htons(_port);
	soc_addr.sin_family = AF_INET;
	
	// bind socket
	int error = bind(_soc, (struct sockaddr*) &soc_addr, sizeof(soc_addr));
	return -(error < 0);
}

/* accepts a connection from a client, and gives the
 * client its data, returns the client fd on success,
 * and a negatvie error code otherwise */
int accept_connection(int _soc, webs_client* _c) {
	static int client_id_counter = 0;
	
	struct sockaddr_in client_addr;
	socklen_t addr_size = sizeof(_c->addr);
	
	_c->fd = accept(_soc, (struct sockaddr*) &_c->addr, &addr_size);
	_c->_is_finished = 0;
	
	if (_c->fd < 0) return -1;
	
	else {
		_c->id = client_id_counter;
		client_id_counter++;
		return _c->fd;
	}
}

/* user function to close socket */
int webs_close(webs_client* _self) {
	webs_remove_client((struct webs_client_node*) _self);
	close(_self->fd);
	
	return 0;
}

/* main client function, called on a thread for each
 * connected client */
int __webs_client_main(webs_client* _self) {
	int cont = 0; // used for handleing continuation frames
	size_t off = 0;
	
	char* data; // used to store data passed to on_data function
	
	// wait for HTTP websocket request header
	_self->buf_recv.len = read(_self->fd, _self->buf_recv.data, PACKET_MAX - 1);
	
	if (_self->buf_recv.len < 0) {
		(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_NO_HANDSHAKE);
		webs_close(_self);
		return 0;
	}
	
	_self->buf_recv.data[_self->buf_recv.len] = '\0';
	
	//process handshake
	struct webs_info ws_info = {0};
	
	if (webs_process_handshake(_self->buf_recv.data, &ws_info) < 0) {
		(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_BAD_REQUEST);
		webs_close(_self);
		return 0;
	}
	
	_self->buf_send.len = webs_generate_handshake(_self->buf_send.data, ws_info.webs_key);
	send(_self->fd, _self->buf_send.data, _self->buf_send.len, 0);
	
	// call clinet on_open function
	(*WEBS_EVENTS.on_open)(_self);
	
	// main loop
	for (;;) {
		// recieve data
		if ((_self->buf_recv.len = read(_self->fd, _self->buf_recv.data, PACKET_MAX - 1)) < 1) break;
		
		// parse data from frame
		struct webs_frame frm;
		webs_parse_frame(_self->buf_recv.data, &frm);
		
		// null string yo
		frm.data[frm.length] = '\0';
		
		// respond to pings
		if (WEBSFR_GET_OPCODE(frm.info) == 0x9) {
			webs_sendn(_self, (char*) &(WORD){0x008A}, 2);
			continue;
		}
		
		// only accept text/binary/continuation frames
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0
		 && WEBSFR_GET_OPCODE(frm.info) != 0x1
		 && WEBSFR_GET_OPCODE(frm.info) != 0x2) continue;
		
		// handle continuation frames
		if (!WEBSFR_GET_FINISH(frm.info)) {
			if (WEBSFR_GET_OPCODE(frm.info) != 0x0)
				data = malloc(frm.length + 1),
				memcpy(data, frm.data, frm.length),
				cont = 1, off = 0;
			
			else
				data = realloc(data, off + frm.length + 1),
				memcpy(data + off, frm.data, frm.length + 1);
			
			webs_decode_data(data + off, frm.key, frm.length);
			off += frm.length;
			
			continue;
		}
		
		// deal with non-fragmented frames
		if (!cont) {
			data = malloc(frm.length + 1);
			memcpy(data, frm.data, frm.length + 1);
			webs_decode_data(data, frm.key, frm.length);
			off = frm.length;
		}
		
		else {
			cont = 0;
		}
		
		// call clinet on_data function
		if (data)
			(*WEBS_EVENTS.on_data)(_self, data, off);
		
		free(data);
		
		continue;
	}
	
	// call client on_close function
	(*WEBS_EVENTS.on_close)(_self);
	
	webs_close(_self);
	
	return 0;
}

/* main loop, listens for connectinos and forks
 * them off for further initialisation */
int __webs_main(int* _soc) {
	pthread_t client_thread;
	webs_client* user_ptr;
	webs_client user;
	
	for (;;) {
		user.fd = accept_connection(*_soc, &user);
		
		if (user.fd >= 0) {
			if (webs_add_client(user, &user_ptr) < 0)
				continue;
			
			pthread_create(&client_thread, 0, (void*) __webs_client_main, user_ptr);
		}
	}
	
	return 0;
}

/* initialises a socket and starts listening for
 * connections: returns a negative error code on
 * error, otherwise 0 */
int webs_start(int _port) {
	int error = 0;
	
	// basic socket setup
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc < 0) return error;
	
	// allow reconnection to socket (for sanity)
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	
	error = bind_address(soc, _port);
	if (error < 0) return error;
	
	error = listen(soc, WEBS_SOCK_BACKLOG_MAX);
	if (error < 0) return error;
	
	// fork further processing to seperate thread
	pthread_create(&__webs_global.thread, 0, (void*) __webs_main, &soc);
	
	return 0;
}
