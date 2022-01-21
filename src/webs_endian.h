#ifndef __WEBS_ENDIAN_H__
#define __WEBS_ENDIAN_H__

#include <stdint.h>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	
	#define BIG_ENDIAN_WORD(X) X
	
	#define BIG_ENDIAN_DWORD(X) X
	
	#define BIG_ENDIAN_QWORD(X) X
	
#else
	
	#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
		#warning Could not determine system endianness (assumng little endian).
	#endif
	
	#define BIG_ENDIAN_WORD(X) (((X << 8) & 0xFF00) | ((X >> 8) & 0x00FF))
	
	#define BIG_ENDIAN_DWORD(X) ((uint32_t) (\
		(((uint32_t) X >> 24) & 0x000000FFUL) |\
		(((uint32_t) X >> 8 ) & 0x0000FF00UL) |\
		(((uint32_t) X << 8 ) & 0x00FF0000UL) |\
		(((uint32_t) X << 24) & 0xFF000000UL)))
	
	#define BIG_ENDIAN_QWORD(X) ( __BIG_ENDIAN_QWORD(X) )
	
#endif

/* 
 * C89 doesn't officially support 64-bt integer constants, so
 * thats why this mess is here...  (there is a better way)
 */
uint64_t __BIG_ENDIAN_QWORD(uint64_t _x);

#endif /* __WEBS_ENDIAN_H__ */
