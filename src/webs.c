#include "webs.h"

/* SSIZE_T MAX */
const size_t WEBS_SSIZE_MAX = (((size_t)(-1)) / 2);

/* header for pong frame (in response to ping) */
const uint8_t PING[2] = {0x89, 0x00};
const uint8_t PONG[2] = {0x8A, 0x00};

const uint8_t WEBSFR_LENGTH_MASK[2] = {0x00, 0x7F};
const uint8_t WEBSFR_OPCODE_MASK[2] = {0x0F, 0x00};
const uint8_t WEBSFR_MASKED_MASK[2] = {0x00, 0x80};
const uint8_t WEBSFR_FINISH_MASK[2] = {0x80, 0x00};
const uint8_t WEBSFR_RESVRD_MASK[2] = {0x70, 0x00};

int webs_default_handler0(struct webs_client* _self) { return 0; }
int webs_default_handler1(struct webs_client* _self, char* _data, ssize_t _n) { return 0; }
int webs_default_handler2(struct webs_client* _self, enum webs_error _ec) { return 0; }
int webs_default_handlerP(struct webs_client* _self) { webs_pong(_self); return 0; }

#include "webs_string.inc"
#include "webs_sha1.inc"
#include "webs_b64.inc"

webs_client* webs_add_client(webs_server* _srv, webs_client _cli) {
	if (!_srv) return 0;
	
	/* if this is first client, set head = tail = new element */
	if (!_srv->tail) {
		_srv->tail = _srv->head = malloc(sizeof(struct webs_client_node));
		
		if (!_srv->tail)
			XERR("Failed to allocate memory!", ENOMEM);
		
		_srv->head->prev = 0;
	}
	
	/* otherwise, just add after the current tail */
	else {
		_srv->tail->next = malloc(sizeof(struct webs_client_node));
		
		if (!_srv->tail->next)
			XERR("Failed to allocate memory!", ENOMEM);
		
		_srv->tail->next->prev = _srv->tail;
		_srv->tail = _srv->tail->next;
	}
	
	_srv->tail->client = _cli;
	_srv->tail->next = 0;
	
	_srv->num_clients++;
	
	return &_srv->tail->client;
}

void webs_remove_client(struct webs_client_node* _node) {
	if (!_node) return;
	
	if (_node->prev)
		_node->prev->next = _node->next;
	
	if (_node->next)
		_node->next->prev = _node->prev;
	
	_node->client.srv->num_clients--;
	free(_node);
	
	return;
}

void webs_eject(webs_client* _self) {
	(*_self->srv->events.on_close)(_self);
	
	close(_self->fd);
	pthread_cancel(_self->thread);
	webs_remove_client((struct webs_client_node*) _self);
	
	return;
}

void webs_close(webs_server* _srv) {
	struct webs_client_node* node = _srv->head;
	struct webs_client_node* temp;
	
	pthread_cancel(_srv->thread);
	close(_srv->soc);
	
	while (node) {
		temp = node->next;
		webs_eject(&node->client);
		node = temp;
	}
	
	free(_srv);
	
	return;
}

size_t webs_flush(int _fd, size_t _n) {
	static char vbuf[512]; /* void buffer */
	short size = 512; /* number of bytes to dispose in next read */
	ssize_t result; /* stores result of read(2) */
	size_t i = 0; /* iteration variable */
	
	/* process data in chunks of 512 bytes, or if the number of
	 * reaining bytes to be read is less than that, update the
	 * chunk size accordingly */
	for (;;) {
		if (_n - i < 512) size = _n - i;
		result = recv(_fd, vbuf, size, MSG_DONTWAIT);
		if (result < 1 || (i += result) >= _n) return i;
	}
}

ssize_t webs_asserted_read(int _fd, void* _dst, size_t _n) {
    size_t i; /* iteration var */
    ssize_t bytes_read;
	size_t size = 32768;
	
    for (i = 0; i < _n;) {
		if (_n - i < size) size = _n - i;
		bytes_read = read(_fd, (char*) _dst + i, size);
		if (bytes_read < 0) return -1;
		else i += bytes_read;
    }
	
    return i;
}

int __webs_parse_frame(webs_client* _self, struct webs_frame* _frm) {
	ssize_t error;
	
	/* read the 2-byte header field */
	error = webs_asserted_read(_self->fd, &_frm->info, 2);
	if (error < 0) return -1; /* read(2) error, maybe broken pipe */
	
	/* read the length field (may offset payload) */
	_frm->off = 2;
	
	/* a value of 126 here says to interpret the next two bytes */
	if (WEBSFR_GET_LENGTH(_frm->info) == 126) {
		error = webs_asserted_read(_self->fd, &_frm->length, 2);
		if (error < 0) return -1; /* read(2) error, maybe broken pipe */
		
		_frm->off = 4;
		_frm->length = BIG_ENDIAN_WORD(_frm->length);
	}
	
	/* a value of 127 says to interpret the next eight bytes */
	else
	if (WEBSFR_GET_LENGTH(_frm->info) == 127) {
		error = webs_asserted_read(_self->fd, &_frm->length, 8);
		if (error < 0) return -1; /* read(2) error, maybe broken pipe */
		
		_frm->off = 10;
		_frm->length = BIG_ENDIAN_QWORD(_frm->length);
	}
	
	/* otherwise, the raw value is used */
	else _frm->length = WEBSFR_GET_LENGTH(_frm->info);
	
	/* if the data is masked, the payload is further offset
	 * to fit a four byte key */
	if (WEBSFR_GET_MASKED(_frm->info)) {
		error = webs_asserted_read(_self->fd, &_frm->key, 4);
		if (error < 1) return -1; /* read(2) error, maybe broken pipe */
	}
	
	/* if it is not masked, then by the specification (RFC-6455), the
	 * connection should be closed */
	else return -1;
	
	/* by the specification (RFC-6455), since no extensions are yet
	 * supported, if we recieve non-zero reserved bits the connection
	 * should be closed */
	if (WEBSFR_GET_RESVRD(_frm->info) != 0) return -1;
	
	return 0;
}

int ____webs_generate_frame(char* _src, char* _dst, ssize_t _n, uint8_t _op) {
	/* offset to the start of the frame's payload */
	short data_start = 2;
	
	uint16_t hdr = 0; /* the frame's header */
	
	WEBSFR_SET_FINISH(hdr, 0x1); /* this is not a cont. frame */
	WEBSFR_SET_OPCODE(hdr, _op); /* opcode */
	
	/* set frame length field */
	
	/* if we have more than 125 bytes, store the length in the
	 * next two bytes */
	if (_n > 125)
		WEBSFR_SET_LENGTH(hdr, 126),
		CASTP(_dst + 2, uint16_t) = BIG_ENDIAN_WORD((uint16_t) _n),
		data_start = 4;
	
	/* if we have more than 2^16 bytes, store the length in
	 * the next eight bytes */
	else
	if (_n > 65536)
		WEBSFR_SET_LENGTH(hdr, 127),
		CASTP(_dst + 2, uint64_t) = BIG_ENDIAN_QWORD((uint64_t) _n),
		data_start = 10;
	
	/* otherwise place the value right in the field */
	else
		WEBSFR_SET_LENGTH(hdr, _n);
	
	/* write header to buffer */
	CASTP(_dst, uint16_t) = hdr;
	
	/* copy data */
	memcpy(_dst + data_start, _src, _n);
	
	return _n + data_start;
}

int webs_decode_data(char* _dta, uint32_t _key, ssize_t _n) {
	ssize_t i;
	
	for (i = 0; i < _n; i++)
		_dta[i] ^= ((char*) &_key)[i % 4];
	
	return 0;
}

/* really ugly, I know... */
int __webs_process_handshake(char* _src, struct webs_info* _rtn) {
	char http_vrs_low = 0;
	
	char param_str[256];
	char req_type[8];
	int nbytes = 0;
	
	_rtn->webs_key[0] = 0;
	_rtn->webs_vrs = 0;
	_rtn->http_vrs = 0;
	
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

int __webs_generate_handshake(char* _dst, char* _key) {
	char buf[61]; /* size of result is 60 bytes */
	char hash[21]; /* SHA-1 hash is 20 bytes */
	int len = 0;
	
	len = str_cat(buf, _key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	webs_sha1(buf, hash, len);
	len = webs_b64_encode(hash, buf, 20);
	buf[len] = '\0';
	
	return sprintf(_dst, WEBS_RESPONSE_FMT, buf);
}

int webs_send(webs_client* _self, char* _data) {
	/* general-purpose recv/send buffer */
	struct webs_buffer soc_buffer = {0};
	
	int len = 0;
	
	/* check for nullptr or empty string */
	if (!_data || !*_data) return 0;
	
	/* get length of data */
	while (_data[++len]);
	
	/* write data */
	return write(
		_self->fd,
		soc_buffer.data,
		____webs_generate_frame(_data, soc_buffer.data, len, 0x1)
	);
	
	return 0;
}

int webs_sendn(webs_client* _self, char* _data, ssize_t _n) {
	/* general-purpose recv/send buffer */
	struct webs_buffer soc_buffer = {0};
	
	/* check for nullptr or empty string */
	if (!_data || !*_data) return 0;
	
	return write(
		_self->fd,
		soc_buffer.data,
		____webs_generate_frame(_data, soc_buffer.data, _n, 0x1)
	);
}

void webs_pong(webs_client* _self) {
	webs_sendn(_self, (char*) &PONG, 2);
	return;
}

int __webs_bind_address(int _soc, int16_t _port) {
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

int __webs_accept_connection(int _soc, webs_client* _c) {
	/* static id counter variable */
	static size_t client_id_counter = 0;
	
	socklen_t addr_size = sizeof(_c->addr);
	
	_c->fd = accept(_soc, (struct sockaddr*) &_c->addr, &addr_size);
	if (_c->fd < 0) return -1;
	
	_c->id = client_id_counter;
	client_id_counter++;
	
	return _c->fd;
}

void __webs_client_main(webs_client* _self) {
	ssize_t total;
	ssize_t error;
	
	/* flag set if frame is a continuation one */
	webs_flag_t cont = 0;
	
	/* general-purpose recv/send buffer */
	struct webs_buffer soc_buffer = {0};
	
	/* temporary variables */
	struct webs_info ws_info;
	struct webs_frame frm;
	char* data;
	
	/* wait for HTTP websocket request header */
	soc_buffer.len = read(_self->fd, soc_buffer.data, WEBS_MAX_PACKET - 1);
	
	/* if we did not recieve one, abort */
	if (soc_buffer.len < 0)
		goto ABORT;
	
	/* process handshake */
	soc_buffer.data[soc_buffer.len] = '\0';
	
	/* if we failed, abort */
	if (__webs_process_handshake(soc_buffer.data, &ws_info) < 0)
		goto ABORT;
	
	/* if we succeeded, generate + tansmit response */
	soc_buffer.len = __webs_generate_handshake(soc_buffer.data, ws_info.webs_key);
	send(_self->fd, soc_buffer.data, soc_buffer.len, 0);
	
	/* call client on_open function */
	(*_self->srv->events.on_open)(_self);
	
	/* main loop */
	for (;;) {
		if (__webs_parse_frame(_self, &frm) < 0) {
			error = WEBS_ERR_READ_FAILED;
			break;
		}
		
		/* only accept supported frames */
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0
		 && WEBSFR_GET_OPCODE(frm.info) != 0x1
		 && WEBSFR_GET_OPCODE(frm.info) != 0x2
		 && WEBSFR_GET_OPCODE(frm.info) != 0x8
		 && WEBSFR_GET_OPCODE(frm.info) != 0x9
		 && WEBSFR_GET_OPCODE(frm.info) != 0xA) {
			(*_self->srv->events.on_error)(_self, WEBS_ERR_NO_SUPPORT);
			webs_flush(_self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* check if packet is too big */
		if ((size_t) frm.length > WEBS_SSIZE_MAX) {
			(*_self->srv->events.on_error)(_self, WEBS_ERR_OVERFLOW);
			webs_flush(_self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* respond to ping */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x9) {
			webs_flush(_self->fd, frm.off + frm.length - 2);
			(*_self->srv->events.on_ping)(_self);
			continue;
		}
		
		/* handle pong */
		if (WEBSFR_GET_OPCODE(frm.info) == 0xA)
			webs_flush(_self->fd, frm.off + frm.length - 2),
			(*_self->srv->events.on_pong)(_self);
		
		/* deal with normal frames (non-fragmented) */
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0) {
			/* read data */
			if (data) free(data);
			data = malloc(frm.length + 1);
			
			if (!data)
				XERR("Failed to allocate memory!", ENOMEM);
			
			if (webs_asserted_read(_self->fd, data, frm.length) < 0) {
				error = WEBS_ERR_READ_FAILED;
				free(data);
				break;
			}
			
			total = frm.length;
			webs_decode_data(data, frm.key, frm.length);
			
			if (!WEBSFR_GET_FINISH(frm.info)) {
				cont = 1;
				continue;
			}
		}
		
		/* otherwise deal with fragmentation */
		else if (cont == 1) {
			data = realloc(data, total + frm.length);
			
			if (!data)
				XERR("Failed to allocate memory!", ENOMEM);
			
			if (webs_asserted_read(_self->fd, data + total, frm.length) < 0) {
				error = WEBS_ERR_READ_FAILED;
				free(data);
				break;
			}
			
			webs_decode_data(data + total, frm.key, frm.length);
			
			total += frm.length;
			
			if (!WEBSFR_GET_FINISH(frm.info))
				continue;
			
			cont = 0;
		}
		
		/* or if we aren't expecting a continuation frame,
		 * set error and skip the frame */
		else {
			(*_self->srv->events.on_error)(_self, WEBS_ERR_UNEXPECTED_CONTINUTATION);
			webs_flush(_self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* respond to close */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x8) {
			soc_buffer.len = ____webs_generate_frame(data, soc_buffer.data, frm.length, 0x8);
			send(_self->fd, soc_buffer.data, soc_buffer.len, 0);
			
			error = 0;
			break;
		}
		
		/* call clinet on_data function */
		data[total] = '\0';
		if (data) (*_self->srv->events.on_data)(_self, data, total);
		free(data);
		
		data = 0;
		continue;
	}
	
	/* call client on_error if there was an error */
	if (error > 0) (*_self->srv->events.on_error)(_self, error);
	
	(*_self->srv->events.on_close)(_self);
	
	ABORT:
	
	close(_self->fd);
	webs_remove_client((struct webs_client_node*) _self);
	
	return;
}

int webs_hold(webs_server* _srv) {
	if (!_srv) return -1;
	return pthread_join(_srv->thread, 0);
}

void __webs_main(webs_server* _srv) {
	webs_client* user_ptr;
	webs_client user;
	
	for (;;) {
		user.fd = __webs_accept_connection(_srv->soc, &user);
		user.srv = _srv;
		
		if (user.fd >= 0)
			user_ptr = webs_add_client(_srv, user),
			pthread_create(&user_ptr->thread, 0, (void*(*)(void*)) __webs_client_main, user_ptr);
	}
	
	return;
}

webs_server* webs_start(int _port) {
	/* static id counter variable */
	static size_t server_id_counter = 0;
	
	const int ONE = 1;
	int error = 0;
	
	webs_server* server = malloc(sizeof(webs_server));
	
	/* basic socket setup */
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc < 0) return 0;
	
	/* allow reconnection to socket (for sanity) */
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(int));
	
	error = __webs_bind_address(soc, _port);
	if (error < 0) return 0;
	
	error = listen(soc, WEBS_MAX_BACKLOG);
	if (error < 0) return 0;
	
	server->soc = soc;
	
	/* initialise default handler */
	server->events.on_error = webs_default_handler2;
	server->events.on_data = webs_default_handler1;
	server->events.on_open = webs_default_handler0;
	server->events.on_close = webs_default_handler0;
	server->events.on_pong = webs_default_handler0;
	server->events.on_ping = 0;
	
	server->id = server_id_counter;
	server_id_counter++;
	
	/* fork further processing to seperate thread */
	pthread_create(&server->thread, 0, (void*(*)(void*)) __webs_main, server);
	
	return server;
}