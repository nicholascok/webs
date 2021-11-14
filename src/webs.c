#include "webs_sha1.h"
#include "webs_b64.h"
#include "webs.h"

/**
 * adds a client to the linked list of connected clients stored
 * in the global `__webs_global` structure.
 * @param `_cli`: the client to be added.
 * @return a pointer to the added client in the linked list. */
webs_client* webs_add_client(webs_client _cli) {
	/* if this is first client, set head = tail = new element */
	if (!__webs_global.tail) {
		__webs_global.tail = __webs_global.head =
			malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail)
			XERR("Failed to allocate memory!", ENOMEM);
		
		__webs_global.head->prev = 0;
	}
	
	/* otherwise, just add after the current tail */
	else {
		__webs_global.tail->next =
			malloc(sizeof(struct webs_client_node));
		
		if (!__webs_global.tail->next)
			XERR("Failed to allocate memory!", ENOMEM);
		
		__webs_global.tail->next->prev = __webs_global.tail;
		__webs_global.tail = __webs_global.tail->next;
	}
	
	__webs_global.tail->client = _cli;
	__webs_global.tail->next = 0;
	
	__webs_global.num_clients++;
	
	return &__webs_global.tail->client;
}

/**
 * removes a client from the linked list of connected clients
 * stored in the global `__webs_global` structure.
 * @param `_node`: a pointer to the element to be removed. */
void webs_remove_client(struct webs_client_node* _node) {
	if (_node->prev)
		_node->prev->next = _node->next;
	
	if (_node->next)
		_node->next->prev = _node->prev;
	
	__webs_global.num_clients--;
	free(_node);
	
	return;
}

/**
 * user function to remove a client (from the linked list
 * of connected clients, close its descriptor, and cancel
 * its thread).
 * @param `_self`: the client to be ejected. */
void webs_eject(webs_client* _self) {
	/* call client on_close function */
	(*WEBS_EVENTS.on_close)(_self);
	
	close(_self->fd);
	pthread_cancel(_self->thread);
	webs_remove_client((struct webs_client_node*) _self);
	
	return;
}

/**
 * shuts down the server (eject all clients, cancel main tread,
 * and close socket). */
void webs_close(void) {
	struct webs_client_node* node = __webs_global.head;
	struct webs_client_node* temp;
	
	pthread_cancel(__webs_global.thread);
	close(__webs_global.soc);
	
	while (node) {
		temp = node->next;
		webs_eject(&node->client);
		node = temp;
	}
	
	return;
}

/**
 * empties bytes from a descriptor's internal buffer.
 * (this is used to skip frames that cannot be processed)
 * @param `_fd`: the descritor whos buffer is to be emptied.
 * @return the number of bytes successfully processed. */
int webs_flush(int _fd, size_t _n) {
	static char vbuf[512]; /* void buffer */
	short size = sizeof(vbuf);
	size_t i = 0; /* iteration variable */
	size_t result;
	
	for (;;) {
		result = recv(_fd, vbuf, size, MSG_DONTWAIT);
		if (result < 1 || (i += result) >= _n) return i;
		if (_n - i < sizeof(vbuf)) size = _n - i;
	}
}

/**
 * parses a websocket frame by reading data sequentially from
 * a socket, storng the result in `_frm`.
 * @param `_self`: a pointer to the client who sent the frame.
 * @param `_frm`: a poiter to store the resulting frame data.
 * @return -1 if there was an error parsing, -2 if there was a
 * fatal error, or 0 otherwise. */
int webs_parse_frame(webs_client* _self, struct webs_frame* _frm) {
	int error;
	
	/* read the 2-byte header field */
	error = read(_self->fd, &_frm->info, 2);
	if (error < 1) return -3; /* read(2) error, maybe broken pipe */
	else if (error < 2) return -1;
	
	/* read the length field (may offset payload) */
	_frm->off = 2;
	
	/* a value of 126 here says to interpret the next two bytes */
	if (WEBSFR_GET_LENGTH(_frm->info) == 126) {
		error = read(_self->fd, &_frm->length, 2);
		if (error < 1) return -3; /* read(2) error, maybe broken pipe */
		else if (error < 2) return -1;
		
		_frm->off = 4;
		_frm->length = BIG_ENDIAN_WORD(_frm->length);
	}
	
	/* a value of 127 says to interpret the next eight bytes */
	else
	if (WEBSFR_GET_LENGTH(_frm->info) == 127) {
		error = read(_self->fd, &_frm->length, 8);
		if (error < 1) return -3; /* read(2) error, maybe broken pipe */
		else if (error < 8) return -1;
		
		_frm->off = 10;
		_frm->length = BIG_ENDIAN_QWORD(_frm->length);
	}
	
	/* otherwise, the raw value is used */
	else _frm->length = WEBSFR_GET_LENGTH(_frm->info);
	
	/* only accept text/binary frames */
	if (WEBSFR_GET_OPCODE(_frm->info) != 0x0
	 && WEBSFR_GET_OPCODE(_frm->info) != 0x1
	 && WEBSFR_GET_OPCODE(_frm->info) != 0x2) {
		webs_flush(_self->fd, _frm->off + _frm->length);
		return -2;
	}
	
	/* if the data is masked, the payload is further offset
	 * to fit a four byte key */
	if (WEBSFR_GET_MASKED(_frm->info)) {
		error = read(_self->fd, &_frm->key, 4);
		if (error < 1) return -3; /* read(2) error, maybe broken pipe */
		else if (error < 4) return -1;
	}
	
	/* by the specification (RFC-6455), since no extensions are yet
	 * supported, if we recieve non-zero reserved bits the connection
	 * should be closed */
	if (WEBSFR_GET_RESVRD(_frm->info) != 0) return -3;
	
	return 0;
}

/**
 * generates a websocket frame from the provided data, and
 * stores the result in `_dst`.
 * @param `_n`: the size of the frame's payload data.
 * @param `_src`: a poiter to the frame's payload data.
 * @param `_dst`: points to a buffer that will hold the
 * resulting frame.
 * @note the caller ensures this buffer is of adequate
 * length, which should never be more than `_n` + 10.
 * @return the total number of resulting bytes copied. */
int webs_generate_frame(char* _src, char* _dst, size_t _n) {
	/* offset to the start of the frame's payload */
	int data_start = 2;
	
	WORD hdr = 0; /* the frame's header */
	
	WEBSFR_SET_FINISH(hdr, 0x1); /* this is not a cont. frame */
	WEBSFR_SET_OPCODE(hdr, 0x1); /* opcode 1 -> text frame */
	
	/* set frame length field */
	
	/* if we have more than 125 bytes, store the length in the
	 * next two bytes */
	if (_n > 125)
		WEBSFR_SET_LENGTH(hdr, 126),
		CAST(_dst + 2, WORD) = BIG_ENDIAN_WORD((WORD) _n),
		data_start = 4;
	
	/* if we have more than SHORT_MAX bytes, store the length in
	 * the next eight bytes */
	else
	if (_n > 65536)
		WEBSFR_SET_LENGTH(hdr, 127),
		CAST(_dst + 2, QWORD) = BIG_ENDIAN_QWORD((QWORD) _n),
		data_start = 10;
	
	/* otherwise place the value right in the field */
	else
		WEBSFR_SET_LENGTH(hdr, _n),
	
	/* write header to buffer */
	CAST(_dst, WORD) = hdr;
	
	/* copy data */
	memcpy(_dst + data_start, _src, _n);
	
	return _n + data_start;
}

/**
 * decodes XOR encrypted data from a websocket frame.
 * @param `_dta`: a pointer to the data that is to be decrypted.
 * @param `_key`: a 32-bit key used to decrypt the data.
 * @param `_n`: the number of bytes of data to be decrypted. */
int webs_decode_data(char* _dta, DWORD _key, size_t _n) {
	size_t i;
	
	for (i = 0; i < _n; i++)
		_dta[i] ^= ((char*) &_key)[i % 4];
	
	return 0;
}

/**
 * tries to parse an HTTP header for web-socket related data.
 * @note this function is a bit of a mess...
 * @param `_src`: a pointer to the header's binary data.
 * @param `_rtn`: a pointer of type `struct webs_info` to store
 * the resulting data.
 * @return 0 on success, or -1 if the frame could not be parsed. */
int webs_process_handshake(char* _src, struct webs_info* _rtn) {
	BYTE http_vrs_low = 0;
	
	char param_str[256];
	char req_type[8];
	int nbytes = 0;
	
	sscanf(_src, "%s %*s HTTP/%c.%c%*[^\r]\r%n", req_type,
		(char*) &_rtn->http_vrs, &http_vrs_low, &nbytes);
	
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

/**
 * generates an HTTP handshake response for a client requesting
 * a websocket connection. By the specification (RFC-6455), this
 * is done by concatonating the key provided by the client in its
 * HTTP request with a magic string and returning the base-64
 * encoded SHA-1 hash of the result in an HTTP response.
 * @param `dst`: a pointer to where the resulting HTTP response
 * data is to be stored.
 * @param `_key`: a pointer to the websocket key provided by the
 * client in its HTTP websocket request.
 * @return the number of resulting bytes written to the buffer. */
int webs_generate_handshake(char* _dst, char* _key) {
	char buf[61 + 19]; /* 61 needed, extra for safety */
	char hash[21];
	int len = 0;
	
	len = str_cat(buf, _key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	webs_sha1(buf, hash, len);
	len = webs_b64_encode(hash, buf, 20);
	buf[len] = '\0';
	
	return sprintf(_dst, WEBS_RESPONSE_FMT, buf);
}

/**
 * user function used to send null-terminated data over a
 * websocket.
 * @param `_self`: the client who is sending the data.
 * @param `_data`: a pointer to the null-terminating data
 * that is to be sent.
 * @return the result of the write. */
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

/**
 * user function used to send binary data over a websocket.
 * @param `_self`: the client that is sending the data.
 * @param `_data`: a pointer to the data to be sent.
 * @param `_n`: the number of bytes that areto be sent.
 * @return the result of the write. */
int webs_sendn(webs_client* _self, char* _data, size_t _n) {
	return write(
		_self->fd,
		_self->buf_send.data,
		webs_generate_frame(_data, _self->buf_send.data, _n)
	);
}

/**
 * binds a socket to an address and port.
 * @param `_soc`: the socket to be bound.
 * @param `_addr`: a null-terminatng string containing the
 * address that tat socket should be bound to as IPV4.
 * @param `_port`: the ort that tat socket should be bound
 * to as a 16-bit integer.
 * @return 0 on success, or a negative error code otherwise */
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

/**
 * accepts a connection from a client, providing it with
 * pertinent data.
 * @param `_soc`: the socket where the connection is occurring.
 * @param `_cli`: the client that is to be connected.
 * @return the client's descriptor on success, or a negatvie
 * error code otherwise */
int accept_connection(int _soc, webs_client* _c) {
	/* static id counter variable */
	static long client_id_counter = 0;
	
	socklen_t addr_size = sizeof(_c->addr);
	
	_c->fd = accept(_soc, (struct sockaddr*) &_c->addr, &addr_size);
	if (_c->fd < 0) return -1;
	
	_c->id = client_id_counter;
	client_id_counter++;
	
	return _c->fd;
}

/**
 * function that is able to read large amountof data
 * (because read(2)'s buffer maxes at 64Kib).
 * @param `_fd`: descritor to read data from.
 * @param `_dst`: destinaton to store data that has
 * been read.
 * @param `_n`: number of bytes to read.
 * @return the number of bytes succesfully read, or
 * 0 on error. */
DWORD webs_big_read(int _fd, char* _dst, size_t _n) {
	size_t off = 0; /* offset from start of `_dst` */
	
	size_t num_rounds = _n / 32768;
	size_t remainder = _n % 32768;
	size_t i;
	
	for (i = 0; i < num_rounds; i++) {
		if (read(_fd, _dst + off, 32768) != 32768) return 0;
		off += 32768;
	}
	
	if ((size_t) read(_fd, _dst + off, remainder) != remainder) return 0;
	
	return off + remainder;
}

/**
 * main client function, called on a thread for each
 * connected client.
 * @param `_self`: the client who is calling. */
void __webs_client_main(webs_client* _self) {
	const uint8_t PING[2] = {0x8A, 0x00};
	size_t total;
	int error = 0;
	int cont = 0;
	
	struct webs_info ws_info = {0};
	struct webs_frame frm;
	
	char* data; /* used to store frame data */
	
	/* wait for HTTP websocket request header */
	_self->buf_recv.len = read(_self->fd, _self->buf_recv.data, WEBS_MAX_SOCKET - 1);
	
	if (_self->buf_recv.len == SIZE_MAX)
		goto ABORT;
	
	/* process handshake */
	_self->buf_recv.data[_self->buf_recv.len] = '\0';
	
	if (webs_process_handshake(_self->buf_recv.data, &ws_info) < 0)
		goto ABORT;
	
	/* generate + tansmit response */
	_self->buf_send.len = webs_generate_handshake(_self->buf_send.data, ws_info.webs_key);
	send(_self->fd, _self->buf_send.data, _self->buf_send.len, 0);
	
	/* call client on_open function */
	(*WEBS_EVENTS.on_open)(_self);
	
	/* main loop */
	for (;;) {
		
		error = webs_parse_frame(_self, &frm);
		if (error == -3) { error = WEBS_ERR_BAD_FRAME; break; }
		
		if (error == -2) {
			(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_BAD_FRAME);
			webs_flush(_self->fd, frm.off + frm.length);
			continue;
		}
		
		if (error == -1) {
			(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_BAD_FRAME);
			webs_flush(_self->fd, 9223372036854775807);
			continue;
		}
		
		/* deal with normal frames (non-fragmented) */
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0) {
			/* read data */
			data = malloc(frm.length + 1);
			
			if (!data)
				XERR("Failed to allocate memory!", ENOMEM);
			
			/* my read buffer maxes at 64Kib, but we will use 48Kib
			 * by default, just in case */
			if (frm.length < WEBS_MAX_SOCKET) {
				if ((size_t) read(_self->fd, data, frm.length) != frm.length)
					{ error = WEBS_ERR_READ_FAILED; free(data); break; }
			}
			
			/* my total tcp memory maxes at 128 Kib, but we will use 96Kib
			 * by default, just in case */
			else if (frm.length < WEBS_MAX_PACKET) {
				if (webs_big_read(_self->fd, data, frm.length) == 0)
					{ error = WEBS_ERR_READ_FAILED; free(data); break; }
			}
			
			/* packet is too big */
			else {
				(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_OVERFLOW);
				webs_flush(_self->fd, 9223372036854775807);
				free(data);
				continue;
			}
			
			if (!WEBSFR_GET_FINISH(frm.info))
				cont = 1, total = frm.length;
		}
		
		/* otherwise deal with continuity */
		else {
			/* if we are not expecting a continuation frame, throw error
			 * (we may have missed a packet) */
			if (!cont) {
				(*WEBS_EVENTS.on_error)(_self, WEBS_NO_CONTEXT);
				/*free(data);*/
				continue;
			}
			
			else {
				data = realloc(data, total + frm.length);
				
				if (!data)
					XERR("Failed to allocate memory!", ENOMEM);
				
				/* my read buffer maxes at 64Kib, but we will use 48Kib
				 * by default, just in case */
				if (frm.length < WEBS_MAX_SOCKET) {
					if ((size_t) read(_self->fd, data + total, frm.length) != frm.length)
						{ error = WEBS_ERR_READ_FAILED; free(data); break; }
				}
				
				/* my total tcp memory maxes at 128 Kib, but we will use 96Kib
				 * by default, just in case */
				else if (frm.length < WEBS_MAX_PACKET) {
					if (webs_big_read(_self->fd, data + total, frm.length) == 0)
						{ error = WEBS_ERR_READ_FAILED; free(data); break; }
				}
				
				/* packet is too big */
				else {
					(*WEBS_EVENTS.on_error)(_self, WEBS_ERR_OVERFLOW);
					webs_flush(_self->fd, 9223372036854775807);
					free(data);
					continue;
				}
				
				total += frm.length;
			}
		}
		
		webs_decode_data(data, frm.key, frm.length);
		
		/* respond to ping */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x9) {
			webs_sendn(_self, (char*) &PING, 2);
			free(data);
			continue;
		}
		
		/* call clinet on_data function */
		data[frm.length] = '\0';
		if (data) (*WEBS_EVENTS.on_data)(_self, data, frm.length);
		free(data);
		
		continue;
	}
	
	/* call client on_error + on_close functions */
	if (error > 0) (*WEBS_EVENTS.on_error)(_self, error);
	
	(*WEBS_EVENTS.on_close)(_self);
	
	ABORT:
	
	close(_self->fd);
	webs_remove_client((struct webs_client_node*) _self);
	
	return;
}

/**
 * blocks until the main server thread closes (likely
 * the server has been closed with `webs_close()`). */
int webs_hold(void) {
	return pthread_join(__webs_global.thread, 0);
}

/**
 * main loop, listens for connectinos and forks
 * them off for further initialisation. */
void __webs_main(void) {
	webs_client* user_ptr;
	webs_client user;
	
	for (;;) {
		user.fd = accept_connection(__webs_global.soc, &user);
		
		if (user.fd >= 0)
			user_ptr = webs_add_client(user),
			pthread_create(&user_ptr->thread, 0, (void*(*)(void*)) __webs_client_main, user_ptr);
	}
	
	return;
}

/**
 * initialises a socket and starts listening for
 * connections:
 * @param `_port`: the port to listen on.
 * @return a negative error code on error, otherwise 0 */
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
