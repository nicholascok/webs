#ifndef __WEBS_ENDIAN_H__
#define __WEBS_ENDIAN_H__

	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		
		#define BIG_ENDIAN_WORD(X) (((X << 8) & 0xFF00) | ((X >> 8) & 0x00FF))
		
		#define BIG_ENDIAN_DWORD(X) ((uint32_t) (\
			(((uint32_t) X >> 24) & 0x000000FFUL) |\
			(((uint32_t) X >> 8 ) & 0x0000FF00UL) |\
			(((uint32_t) X << 8 ) & 0x00FF0000UL) |\
			(((uint32_t) X << 24) & 0xFF000000UL)))
		
		/* C89 doesn't officially support 64-bt integer constants, so
		 * thats why this mess is here...  (there is a better way) */
		uint64_t BIG_ENDIAN_QWORD(uint64_t _x) {
			((uint32_t*) &_x)[0] = BIG_ENDIAN_DWORD(((uint32_t*) &_x)[0]);
			((uint32_t*) &_x)[1] = BIG_ENDIAN_DWORD(((uint32_t*) &_x)[1]);
			((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
			((uint32_t*) &_x)[1] ^= ((uint32_t*) &_x)[0];
			((uint32_t*) &_x)[0] ^= ((uint32_t*) &_x)[1];
			return _x;
		}
		
	#else
		
		#define BIG_ENDIAN_WORD(X) X
		
		#define BIG_ENDIAN_DWORD(X) X
		
		#define BIG_ENDIAN_QWORD(X) X
		
	#endif

#endif