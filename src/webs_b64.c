#include "webs_b64.h"

int webs_b64_encode(char* _s, char* _d, size_t _n) {
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
	if (rem == 1)
		_d[i + 0] = TO_B64((_s[0] & 0xFC) >> 2),
		_d[i + 1] = TO_B64((_s[0] & 0x03) << 4),
		_d[i + 2] = '=',
		_d[i + 3] = '=';
	
	else
	if (rem == 2)
		_d[i + 0] = TO_B64(( _s[0] & 0xFC) >> 2),
		_d[i + 1] = TO_B64(((_s[0] & 0x03) << 4) | ((_s[1] & 0xF0) >> 4)),
		_d[i + 2] = TO_B64(( _s[1] & 0x0F) << 2),
		_d[i + 3] = '=';
	
	_d[i + 4] = '\0';
	
	return i + 4;
}

int webs_b64_decode(char* _s, char* _d) {
	_s = _d; /* these are here to avoid compiler warnings while */
	_d = _s; /* this function is under development. (it is never called) */
	
	return 0;
}
