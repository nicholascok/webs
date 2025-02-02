#include "webs.h"

/* headers for ping and pong frames */
uint8_t WEBS_PING[2] = {0x89, 0x00};
uint8_t WEBS_PONG[2] = {0x8A, 0x00};

/* masks... */
uint8_t WEBSFR_LENGTH_MASK[2] = {0x00, 0x7F};
uint8_t WEBSFR_OPCODE_MASK[2] = {0x0F, 0x00};
uint8_t WEBSFR_MASKED_MASK[2] = {0x00, 0x80};
uint8_t WEBSFR_FINISH_MASK[2] = {0x80, 0x00};
uint8_t WEBSFR_RESVRD_MASK[2] = {0x70, 0x00};

/* 
 * strcat that write result to a buffer...
 */
static int __webs_strcat(char* _buf, char* _a, char* _b) {
	int len = 0;
	while (*_a) *(_buf++) = *(_a++), len++;
	while (*_b) *(_buf++) = *(_b++), len++;
	*_buf = '\0';
	return len;
}

/* 
 * C89 doesn't officially support 64-bt integer constants, so
 * thats why this is here...
 */
uint64_t __WEBS_BIG_ENDIAN_QWORD(uint64_t _x) {
	((uint32_t*) &_x)[0] = WEBS_BIG_ENDIAN_DWORD(((uint32_t*) &_x)[0]);
	((uint32_t*) &_x)[1] = WEBS_BIG_ENDIAN_DWORD(((uint32_t*) &_x)[1]);
	((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
	((uint32_t*) &_x)[1] ^= ((uint32_t*) &_x)[0];
	((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
	return _x;
}

/* 
 * takes the SHA-1 hash of `_n` bytes of data (pointed to by `_s`),
 * storing the 160-bit (20-byte) result in the buffer pointed to by `_d`.
 */
static int __webs_sha1(char* _s, char* _d, uint64_t _n) {
	uint64_t raw_bits = _n * 8;     	/* length of message in bits */
	uint32_t a, b, c, d, e, f, k, t;	/* internal temporary variables */
	uint64_t C, O, i;               	/* iteration variables */
	short j;                        	/* likewise */
	
	uint8_t* ptr = (uint8_t*) _s;	/* pointer to active chunk data */
	uint32_t wrds[80];           	/* used in main loop */
	
	/* constants */
	uint32_t h0 = 0x67452301;
	uint32_t h1 = 0xEFCDAB89;
	uint32_t h2 = 0x98BADCFE;
	uint32_t h3 = 0x10325476;
	uint32_t h4 = 0xC3D2E1F0;
	
	/* pad the message length (plus one so that a bit can
	 * be appended to the message, as per the specification)
	 * so it is congruent to 56 modulo 64, all in bytes,
	 * then add 8 bytes for the 64-bit length field */
	
	/* equivelantly, ((56 - (_n + 1)) MOD 64) + (_n + 1) + 8,
	 * where `a MOD b` is the POSITIVE remainder after a is
	 * divided by b */
	uint64_t pad_n = _n + ((55 - _n) & 63) + 9;
	
	uint64_t num_chks = pad_n / 64;	/* number of chunks to be processed */
	uint16_t rem_chks_begin = 0;   	/* the first chunk with extended data */
	uint16_t offset = pad_n - 128; 	/* start index for extended data */
	
	/* buffer to store extended chunk data (i.e. data that goes past the
	 * smallest multiple of 64 bytes less than `_n`) (to avoid having to
	 * make an allocation)*/
	uint8_t ext[128] = {0};
	
	/* if n is less than 120 bytes, then we can store the expanded data
	 * directly in our buffer */
	if (_n < 120) {
		*((uint64_t*) &ext[pad_n - 8]) = WEBS_BIG_ENDIAN_QWORD(raw_bits);
		ext[_n] = 0x80;
		
		for (i = 0; i < _n; i++)
			ext[i] = _s[i];
	}
	
	/* otherwise, we will save our buffer for the last two chunks */
	else {
		rem_chks_begin = num_chks - 2;
		
		ext[_n - offset] = 0x80;
		*((uint64_t*) &ext[120]) = WEBS_BIG_ENDIAN_QWORD(raw_bits);
		
		for (i = 0; i < _n - offset; i++)
			ext[i] = _s[offset + i];
	}
	
	/* main loop (very similar to example in specification) */
	for (C = O = 0; C < num_chks; C++, O++) {
		if (C == rem_chks_begin) ptr = ext, O = 0;
		
		for (j = 0; j < 16; j++)
			wrds[j] = WEBS_BIG_ENDIAN_DWORD(((uint32_t*) ptr)[j]);
		
		for (j = 16; j < 80; j++) 
			wrds[j] = ROL((wrds[j - 3] ^ wrds[j - 8] ^
				wrds[j - 14] ^ wrds[j - 16]), 1);
		
		a = h0; b = h1; c = h2;
		d = h3; e = h4;
		
		for (j = 0; j < 80; j++) {
			if (j < 20) {
				f = ((b & c) | ((~b) & d));
				k = 0x5A827999;
			} else if (j < 40) {
				f = (b ^ c ^ d);
				k = 0x6ED9EBA1;
			} else if (j < 60) {
				f = ((b & c) | (b & d) | (c & d));
				k = 0x8F1BBCDC;
			} else {
				f = (b ^ c ^ d);
				k = 0xCA62C1D6;
			}
			
			t = ROL(a, 5) + f + e + k + wrds[j];
			
			e = d; d = c;
			c = ROL(b, 30);
			b = a; a = t;
		}
		
		h0 = h0 + a; h1 = h1 + b;
		h2 = h2 + c; h3 = h3 + d;
		h4 = h4 + e;
		
		ptr += 64;
	}
	
	/* copy data into destination */
	((uint32_t*) _d)[0] = WEBS_BIG_ENDIAN_DWORD(h0);
	((uint32_t*) _d)[1] = WEBS_BIG_ENDIAN_DWORD(h1);
	((uint32_t*) _d)[2] = WEBS_BIG_ENDIAN_DWORD(h2);
	((uint32_t*) _d)[3] = WEBS_BIG_ENDIAN_DWORD(h3);
	((uint32_t*) _d)[4] = WEBS_BIG_ENDIAN_DWORD(h4);
	
	return 0;
}

/* 
 * Encodes `_n` bytes of data (pointed to by `_s`) into
 * base-64, storing the result as a null terminating string
 * in `_d`. The number of successfully written bytes is
 * returned.
 */
static int __webs_b64_encode(char* _s, char* _d, size_t _n) {
	size_t i = 0; /* iteration variable */
	
	/* `n_max` is the number of base-64 chars we
	 * expect to output in the main loop */
	size_t n_max;
	
	/* belay data past a multiple of 3 bytes - so
	 * we end on a whole number of encoded chars in
	 * the main loop. */
	int rem = _n % 3;
	_n -= rem;
	
	n_max = (_n * 4) / 3;
	
	/* process bulk of data */
	while (i < n_max) {
		_d[i + 0] = TO_B64(( _s[0] & 0xFC) >> 2);
		_d[i + 1] = TO_B64(((_s[0] & 0x03) << 4) | ((_s[1] & 0xF0) >> 4));
		_d[i + 2] = TO_B64(((_s[1] & 0x0F) << 2) | ((_s[2] & 0xC0) >> 6));
		_d[i + 3] = TO_B64(  _s[2] & 0x3F);
		_s += 3, i += 4;
	}
	
	/* deal with remaining bytes (some may need to
	 * be 0-padded if the data is not a multiple of
	 * 6-bits - the length of a base-64 digit) */
	if (rem == 1) {
		_d[i + 0] = TO_B64((_s[0] & 0xFC) >> 2);
		_d[i + 1] = TO_B64((_s[0] & 0x03) << 4);
		_d[i + 2] = '=';
		_d[i + 3] = '=';
	} else if (rem == 2) {
		_d[i + 0] = TO_B64(( _s[0] & 0xFC) >> 2);
		_d[i + 1] = TO_B64(((_s[0] & 0x03) << 4) | ((_s[1] & 0xF0) >> 4));
		_d[i + 2] = TO_B64(( _s[1] & 0x0F) << 2);
		_d[i + 3] = '=';
	}
	
	_d[i + 4] = '\0';
	
	return i + 4;
}

/* 
 * wraper functon that deals with reading lage amounts
 * of data, as well as attemts to complete partial reads.
 * @param _fd: the file desciptor to be read from.
 * @param _dst: a buffer to store the resulting data.
 * @param _n: the number of bytes to be read.
 */
static ssize_t __webs_asserted_read(int _fd, void* _dst, size_t _n) {
	ssize_t bytes_read;
	size_t size = 32768;
	size_t i;
	
	for (i = 0; i < _n;) {
		if (_n - i < size)
			size = _n - i;
		
		bytes_read = read(_fd, (char*) _dst + i, size);
		
		if (bytes_read < 0)
			return -1;
		
		i += bytes_read;
	}
	
	return i;
}

/* 
 * decodes XOR encrypted data from a websocket frame.
 * @param _dta: a pointer to the data that is to be decrypted.
 * @param _key: a 32-bit key used to decrypt the data.
 * @param _n: the number of bytes of data to be decrypted.
 */
static int __webs_decode_data(char* _dta, uint32_t _key, ssize_t _n) {
	ssize_t i;
	
	for (i = 0; i < _n; i++)
		_dta[i] ^= ((char*) &_key)[i % 4];
	
	return 0;
}

/* 
 * empties bytes from a descriptor's internal buffer.
 * (this is used to skip frames that cannot be processed)
 * @param _fd: the descritor whos buffer is to be emptied.
 * @return the number of bytes successfully processed.
 */
static size_t __webs_flush(int _fd, size_t _n) {
	static char vbuf[512];	/* void buffer */
	short size = 512;     	/* number of bytes to dispose in next read */
	ssize_t result;       	/* stores result of read(2) */
	size_t i;
	
	/* process data in chunks of 512 bytes, or if the number of
	 * reaining bytes to be read is less than that, update the
	 * chunk size accordingly */
	for (i = 0;;) {
		if (_n - i < 512)
			size = _n - i;
		
		result = recv(_fd, vbuf, size, MSG_DONTWAIT);
		
		if (result < 1 || (i += result) >= _n)
			return i;
	}
}

/* 
 * parses a websocket frame by reading data sequentially from
 * a socket, storing the result in `_frm`.
 * @param _self: a pointer to the client who sent the frame.
 * @param _frm: a poiter to store the resulting frame data.
 * @return -1 if the frame could not be parsed, or 0 otherwise.
 */
static int __webs_parse_frame(webs_client* _self, struct webs_frame* _frm) {
	ssize_t error;
	
	/* read the 2-byte header field */
	error = __webs_asserted_read(_self->fd, &_frm->info, 2);
	if (error < 0) return -1; /* read(2) error, maybe broken pipe */
	
	/* read the length field (may offset payload) */
	_frm->off = 2;
	
	/* a value of 126 here says to interpret the next two bytes */
	if (WEBSFR_GET_LENGTH(_frm->info) == 126) {
		error = __webs_asserted_read(_self->fd, &_frm->length, 2);
		if (error < 0) return -1; /* read(2) error, maybe broken pipe */
		
		_frm->off = 4;
		_frm->length = WEBS_BIG_ENDIAN_WORD(_frm->length);
	}
	
	/* a value of 127 says to interpret the next eight bytes */
	else if (WEBSFR_GET_LENGTH(_frm->info) == 127) {
		error = __webs_asserted_read(_self->fd, &_frm->length, 8);
		if (error < 0) return -1; /* read(2) error, maybe broken pipe */
		
		_frm->off = 10;
		_frm->length = WEBS_BIG_ENDIAN_QWORD(_frm->length);
	}
	
	/* otherwise, the raw value is used */
	else _frm->length = WEBSFR_GET_LENGTH(_frm->info);
	
	/* if the data is masked, the payload is further offset
	 * to fit a four byte key */
	if (WEBSFR_GET_MASKED(_frm->info)) {
		error = __webs_asserted_read(_self->fd, &_frm->key, 4);
		if (error < 1) return -1; /* read(2) error, maybe broken pipe */
	}
	
	/* if it is not masked, then by the specification (RFC-6455), the
	 * connection should be closed */
	else return -1;
	
	/* by the specification (RFC-6455), since no extensions are yet
	 * supported, if we recieve non-zero reserved bits the connection
	 * should be closed */
	if (WEBSFR_GET_RESVRD(_frm->info) != 0)
		return -1;
	
	return 0;
}

/* 
 * generates a websocket frame from the provided data.
 * @param _src: a pointer to the frame's payload data.
 * @param _dst: a buffer that will hold the resulting frame.
 * @note the caller ensures this buffer is of adequate
 * length (it shouldn't need more than _n + 10 bytes).
 * @param _n: the size of the frame's payload data.
 * @param _op: the frame's opcode.
 * @return the total number of resulting bytes copied.
 */
static int __webs_make_frame(char* _src, char* _dst, ssize_t _n, uint8_t _op) {
	short data_start = 2;	/* offset to the start of the frame's payload */
	uint16_t hdr = 0;    	/* the frame's header */
	
	WEBSFR_SET_FINISH(hdr, 0x1);	/* this is not a cont. frame */
	WEBSFR_SET_OPCODE(hdr, _op);	/* opcode */
	
	/* set frame length field */
	
	/* if we have more than 125 bytes, store the length in the
	 * next two bytes */
	if (_n > 125) {
		WEBSFR_SET_LENGTH(hdr, 126);
		CASTP(_dst + 2, uint16_t) = WEBS_BIG_ENDIAN_WORD((uint16_t) _n);
		data_start = 4;
	}
	
	/* if we have more than 2^16 bytes, store the length in
	 * the next eight bytes */
	else if (_n > 65536) {
		WEBSFR_SET_LENGTH(hdr, 127);
		CASTP(_dst + 2, uint64_t) = WEBS_BIG_ENDIAN_QWORD((uint64_t) _n);
		data_start = 10;
	}
	
	/* otherwise place the value right in the field */
	else WEBSFR_SET_LENGTH(hdr, _n);
	
	/* write header to buffer */
	CASTP(_dst, uint16_t) = hdr;
	
	/* copy data */
	memcpy(_dst + data_start, _src, _n);
	
	return _n + data_start;
}

/* 
 * parses an HTTP header for web-socket related data.
 * @note this function is a bit of a mess...
 * @param _src: a pointer to the raw header data.
 * @param _rtn: a pointer to store the resulting data.
 * @return -1 on error (bad vers., ill-formed, etc.), or 0
 * otherwise.
 */
static int __webs_process_handshake(char* _src, struct webs_info* _rtn) {
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
	
	if (strcmp(req_type, "GET"))
		return -1;
	
	_rtn->http_vrs <<= 8;
	_rtn->http_vrs += http_vrs_low;
	
	while (sscanf(_src, "%s%n", param_str, &nbytes) > 0) {
		_src += nbytes;
		
		if (!strcmp(param_str, "Sec-WebSocket-Version:")) {
			sscanf(_src, "%hu%*[^\r]\r%n", &_rtn->webs_vrs, &nbytes);
			_src += nbytes;
		}
		
		else
		if (!strcmp(param_str, "Sec-WebSocket-Key:")) {
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

/* 
 * generates an HTTP websocket handshake response. by the
 * specification (RFC-6455), this is done by concatonating the
 * client provided key with a magic string, and returning the
 * base-64 encoded, SHA-1 hash of the result in the "Sec-WebSocket-
 * Accept" field of an HTTP response header.
 * @param _dst: a buffer that will hold the resulting HTTP
 * response data.
 * @param _key: a pointer to the websocket key provided by the
 * client in it's HTTP websocket request header.
 * @return the total number of resulting bytes copied.
 */
static int __webs_generate_handshake(char* _dst, char* _key) {
	char buf[61]; 	/* size of result is 60 bytes */
	char hash[21];	/* SHA-1 hash is 20 bytes */
	int len = 0;
	
	len = __webs_strcat(buf, _key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
	__webs_sha1(buf, hash, len);
	len = __webs_b64_encode(hash, buf, 20);
	buf[len] = '\0';
	
	return sprintf(_dst, WEBS_RESPONSE_FMT, buf);
}

/* 
 * removes a client from a server's internal listing.
 * @param _node: a pointer to the client in the server's listing.
 */
static void __webs_remove_client(struct webs_client_node* _node) {
	if (_node == NULL) return;
	
	if (_node->prev)
		_node->prev->next = _node->next;
	
	if (_node->next)
		_node->next->prev = _node->prev;
	
	_node->client.srv->num_clients--;
	free(_node);
	
	return;
}

/* 
 * adds a client to a server's internal listing.
 * @param _srv: the server that the client should be added to.
 * @param _cli: the client to be added.
 * @return a pointer to the added client in the server's listing.
 * (or NULL if NULL was provided)
 */
static webs_client* __webs_add_client(webs_server* _srv, webs_client _cli) {
	if (_srv == NULL) return NULL;
	
	/* if this is first client, set head = tail = new element */
	if (_srv->tail == NULL) {
		_srv->tail = _srv->head = malloc(sizeof(struct webs_client_node));
		
		if (_srv->tail == NULL)
			WEBS_XERR("Failed to allocate memory!", ENOMEM);
		
		_srv->head->prev = NULL;
	}
	
	/* otherwise, just add after the current tail */
	else {
		_srv->tail->next = malloc(sizeof(struct webs_client_node));
		
		if (_srv->tail->next == NULL)
			WEBS_XERR("Failed to allocate memory!", ENOMEM);
		
		_srv->tail->next->prev = _srv->tail;
		_srv->tail = _srv->tail->next;
	}
	
	_srv->tail->client = _cli;
	_srv->tail->next = NULL;
	
	_srv->num_clients++;
	
	return &_srv->tail->client;
}

/* 
 * binds a socket to an address and port.
 * @param _soc: the socket to be bound.
 * @param _addr: a null-terminatng string containing the
 * address that the socket should be bound to.
 * @param _port: the port that the socket should be bound
 * to as a 16-bit integer.
 * @return -1 on error, or 0 otherwise.
 */
static int __webs_bind_address(int _soc, int16_t _port) {
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

/* 
 * accepts a connection from a client and provides it with
 * relevant data.
 * @param _soc: the socket that the connection is being requested on.
 * @param _cli: the client that is to be connected.
 * @return -1 on error, or 0 otherwise.
 */
static int __webs_accept_connection(int _soc, webs_client* _c) {
	/* static id counter variable */
	static size_t client_id_counter = 0;
	
	socklen_t addr_size = sizeof(_c->addr);
	
	_c->fd = accept(_soc, (struct sockaddr*) &_c->addr, &addr_size);
	if (_c->fd < 0) return -1;
	
	_c->id = client_id_counter;
	client_id_counter++;
	
	return _c->fd;
}

/* 
 * main client function, called on a thread for each
 * connected client.
 * @param _self: the client who is calling.
 */
static void* __webs_client_main(void* _self) {
	webs_client* self = (webs_client*) _self;
	ssize_t total;
	ssize_t error;
	
	/* flag set if frame is a continuation one */
	int cont = 0;
	
	/* general-purpose recv/send buffer */
	struct webs_buffer soc_buffer = {0};
	
	/* temporary variables */
	struct webs_info ws_info;
	struct webs_frame frm;
	char* data = 0;
	
	/* wait for HTTP websocket request header */
	soc_buffer.len = read(self->fd, soc_buffer.data, WEBS_MAX_PACKET - 1);
	
	/* if we did not recieve one, abort */
	if (soc_buffer.len < 0)
		goto ABORT;
	
	/* process handshake */
	soc_buffer.data[soc_buffer.len] = '\0';
	
	/* if we failed, abort */
	if (__webs_process_handshake(soc_buffer.data, &ws_info) < 0)
		goto ABORT;
	
	/* if we succeeded, generate + tansmit response */
	soc_buffer.len = __webs_generate_handshake(soc_buffer.data,
		ws_info.webs_key);
	
	send(self->fd, soc_buffer.data, soc_buffer.len, 0);
	
	/* call client on_open function */
	if (*self->srv->events.on_open)
		(*self->srv->events.on_open)(self);
	
	/* main loop */
	for (;;) {
		if (__webs_parse_frame(self, &frm) < 0) {
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
			if (*self->srv->events.on_error)
				(*self->srv->events.on_error)(self, WEBS_ERR_NO_SUPPORT);
			
			__webs_flush(self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* check if packet is too big */
		if ((size_t) frm.length > SSIZE_MAX) {
			if (*self->srv->events.on_error)
				(*self->srv->events.on_error)(self, WEBS_ERR_OVERFLOW);
			
			__webs_flush(self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* respond to ping */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x9) {
			__webs_flush(self->fd, frm.off + frm.length - 2);
			
			if (*self->srv->events.on_ping)
				(*self->srv->events.on_ping)(self);
			
			else
				webs_pong(self);
			
			continue;
		}
		
		/* handle pong */
		if (WEBSFR_GET_OPCODE(frm.info) == 0xA) {
			__webs_flush(self->fd, frm.off + frm.length - 2);
			
			if (*self->srv->events.on_pong)
				(*self->srv->events.on_pong)(self);
		}
		
		/* deal with normal frames (non-fragmented) */
		if (WEBSFR_GET_OPCODE(frm.info) != 0x0) {
			/* read data */
			if (data) free(data);
			data = malloc(frm.length + 1);
			
			if (data == NULL)
				WEBS_XERR("Failed to allocate memory!", ENOMEM);
			
			if (__webs_asserted_read(self->fd, data, frm.length) < 0) {
				error = WEBS_ERR_READ_FAILED;
				free(data);
				break;
			}
			
			total = frm.length;
			__webs_decode_data(data, frm.key, frm.length);
			
			if (!WEBSFR_GET_FINISH(frm.info)) {
				cont = 1;
				continue;
			}
		}
		
		/* otherwise deal with fragmentation */
		else if (cont == 1) {
			data = realloc(data, total + frm.length);
			
			if (data == NULL)
				WEBS_XERR("Failed to allocate memory!", ENOMEM);
			
			if (__webs_asserted_read(self->fd, data + total,
			frm.length) < 0) {
				error = WEBS_ERR_READ_FAILED;
				free(data);
				break;
			}
			
			__webs_decode_data(data + total, frm.key, frm.length);
			
			total += frm.length;
			
			if (!WEBSFR_GET_FINISH(frm.info))
				continue;
			
			cont = 0;
		}
		
		/* or if we aren't expecting a continuation frame,
		 * set error and skip the frame */
		else {
			if (*self->srv->events.on_error)
				(*self->srv->events.on_error)(self,
					WEBS_ERR_UNEXPECTED_CONTINUTATION);
			
			__webs_flush(self->fd, frm.off + frm.length - 2);
			continue;
		}
		
		/* respond to close */
		if (WEBSFR_GET_OPCODE(frm.info) == 0x8) {
			soc_buffer.len = __webs_make_frame(data, soc_buffer.data,
				frm.length, 0x8);
			
			send(self->fd, soc_buffer.data, soc_buffer.len, 0);
			
			error = 0;
			break;
		}
		
		/* call clinet on_data function */
		data[total] = '\0';
		
		if (data) {
			if (*self->srv->events.on_data)
				(*self->srv->events.on_data)(self, data, total);
		}
		
		free(data);
		
		data = 0;
		continue;
	}
	
	/* call client on_error if there was an error */
	if (error > 0) {
		if (*self->srv->events.on_error)
			(*self->srv->events.on_error)(self, error);
	}
	
	if (*self->srv->events.on_close)
		(*self->srv->events.on_close)(self);
	
	ABORT:
	
	close(self->fd);
	__webs_remove_client((struct webs_client_node*) self);
	
	return NULL;
}

/* 
 * main loop for a server, listens for connections and forks
 * them off for further initialisation.
 * @param _srv: the server that is calling.
 */
static void* __webs_main(void* _srv) {
	webs_server* srv = (webs_server*) _srv;
	webs_client* user_ptr;
	webs_client user;
	
	for (;;) {
		user.fd = __webs_accept_connection(srv->soc, &user);
		user.srv = srv;
		
		if (user.fd >= 0) {
			user_ptr = __webs_add_client(srv, user);
			pthread_create(&user_ptr->thread, 0, __webs_client_main, user_ptr);
		}
	}
	
	return NULL;
}

void webs_eject(webs_client* _self) {
	if (*_self->srv->events.on_close)
		(*_self->srv->events.on_close)(_self);
	
	close(_self->fd);
	pthread_cancel(_self->thread);
	__webs_remove_client((struct webs_client_node*) _self);
	
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
		__webs_make_frame(_data, soc_buffer.data, len, 0x1)
	);
	
	return 0;
}

int webs_sendn(webs_client* _self, char* _data, ssize_t _n) {
	/* general-purpose recv/send buffer */
	struct webs_buffer soc_buffer = {0};
	
	/* check for NULL or empty string */
	if (!_data || !*_data) return 0;
	
	return write(
		_self->fd,
		soc_buffer.data,
		__webs_make_frame(_data, soc_buffer.data, _n, 0x1)
	);
}

void webs_pong(webs_client* _self) {
	webs_sendn(_self, (char*) &WEBS_PONG, 2);
	return;
}

int webs_hold(webs_server* _srv) {
	if (_srv == NULL) return -1;
	return pthread_join(_srv->thread, 0);
}

webs_server* webs_start(int _port) {
	/* static id counter variable */
	static size_t server_id_counter = 0;
	
	const int ONE = 1;
	int error = 0;
	
	webs_server* server = malloc(sizeof(webs_server));
	
	/* basic socket setup */
	int soc = socket(AF_INET, SOCK_STREAM, 0);
	if (soc < 0) return NULL;
	
	/* allow reconnection to socket (for sanity) */
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &ONE, sizeof(int));
	
	error = __webs_bind_address(soc, _port);
	if (error < 0) return NULL;
	
	error = listen(soc, WEBS_MAX_BACKLOG);
	if (error < 0) return NULL;
	
	server->soc = soc;
	
	/* initialise default handlers */
	server->events.on_error = NULL;
	server->events.on_data  = NULL;
	server->events.on_open  = NULL;
	server->events.on_close = NULL;
	server->events.on_pong  = NULL;
	server->events.on_ping  = NULL;
	
	server->id = server_id_counter;
	server_id_counter++;
	
	/* fork further processing to seperate thread */
	pthread_create(&server->thread, 0, __webs_main, server);
	
	return server;
}
