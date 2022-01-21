#ifndef __WEBS_SHA1_H__
#define __WEBS_SHA1_H__

#include <stdint.h>
#include "webs_endian.h"

/* 
 * bitwise rotate left
 */
#define ROL(X, N) ((X << N) | (X >> ((sizeof(X) * 8) - N)))

/* 
 * takes the SHA-1 hash of `_n` bytes of data (pointed to by `_s`), storing
 * the 160-bit (20-byte) result in the buffer pointed to by `_d`.
 */
int webs_sha1(char* _s, char* _d, uint64_t _n);

#endif /* __WEBS_SHA1_H__ */
