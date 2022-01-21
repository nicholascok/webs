#include "webs_endian.h"

uint64_t __BIG_ENDIAN_QWORD(uint64_t _x) {
	((uint32_t*) &_x)[0] = BIG_ENDIAN_DWORD(((uint32_t*) &_x)[0]);
	((uint32_t*) &_x)[1] = BIG_ENDIAN_DWORD(((uint32_t*) &_x)[1]);
	((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
	((uint32_t*) &_x)[1] ^= ((uint32_t*) &_x)[0];
	((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
	return _x;
}
