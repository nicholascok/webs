#include "webs_sha1.h"

int webs_sha1(char* _s, char* _d, uint64_t _n) {
	uint64_t raw_bits = _n * 8; /* length of message in bits */
	uint32_t a, b, c, d, e, f, k, t; /* internal temporary variables */
	uint64_t C, O, i; /* iteration variables */
	short j; /* likewise */
	
	uint32_t wrds[80]; /* used in main loop */
	uint8_t* ptr = (uint8_t*) _s; /* pointer to active chunk data */
	
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
	
	/* equivelanl, ((56 - (_n + 1)) MOD 64) + (_n + 1) + 8,
	 * where `a MOD b` is the POSITIVE remainder after a is
	 * divided by b */
	uint64_t pad_n = _n + ((55 - _n) & 63) + 9;
	
	uint64_t num_chks = pad_n / 64; /* number of chunks to be rocessed */
	uint16_t rem_chks_begin = 0; /* the first chunk where extended data is used */
	uint16_t offset = pad_n - 128; /* start index for overflow into extended data */
	
	/* buffer to store extended chunk data (i.e. data that goes past the smallest
	* multiple of 64 bytes less than `_n`) (to avoid having to make an allocation)*/
	uint8_t ext[128] = {0};
	
	/* if n is less than 120 bytes, then we can store the expanded data
	 * directly in our buffer */
	if (_n < 120) {
		*((uint64_t*) &ext[pad_n - 8]) = BIG_ENDIAN_QWORD(raw_bits);
		ext[_n] = 0x80;
		
		for (i = 0; i < _n; i++)
			ext[i] = _s[i];
	}
	
	/* otherwise, we will save our buffer for the last two chunks */
	else {
		rem_chks_begin = num_chks - 2;
		
		ext[_n - offset] = 0x80;
		*((uint64_t*) &ext[120]) = BIG_ENDIAN_QWORD(raw_bits);
		
		for (i = 0; i < _n - offset; i++)
			ext[i] = _s[offset + i];
	}
	
	/* main loop (very similar to example in specification) */
	for (C = O = 0; C < num_chks; C++, O++) {
		if (C == rem_chks_begin) ptr = ext, O = 0;
		
		for (j = 0; j < 16; j++)
			wrds[j] = BIG_ENDIAN_DWORD(((uint32_t*) ptr)[j]);
		
		for (j = 16; j < 80; j++) 
			wrds[j] = ROL((wrds[j - 3] ^ wrds[j - 8] ^
				wrds[j - 14] ^ wrds[j - 16]), 1);
		
		a = h0; b = h1; c = h2;
		d = h3; e = h4;
		
		for (j = 0; j < 80; j++) {
			if (j < 20)
				f = ((b & c) | ((~b) & d)),
				k = 0x5A827999;
			
			else
			if (j < 40)
				f = (b ^ c ^ d),
				k = 0x6ED9EBA1;
			
			else
			if (j < 60)
				f = ((b & c) | (b & d) | (c & d)),
				k = 0x8F1BBCDC;
			
			else
				f = (b ^ c ^ d),
				k = 0xCA62C1D6;
			
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
	((uint32_t*) _d)[0] = BIG_ENDIAN_DWORD(h0);
	((uint32_t*) _d)[1] = BIG_ENDIAN_DWORD(h1);
	((uint32_t*) _d)[2] = BIG_ENDIAN_DWORD(h2);
	((uint32_t*) _d)[3] = BIG_ENDIAN_DWORD(h3);
	((uint32_t*) _d)[4] = BIG_ENDIAN_DWORD(h4);
	
	return 0;
}
