#ifndef __WEBS_BASE_64_H__
#define __WEBS_BASE_64_H__

#if !defined(_SIZE_T) && !defined(_SIZE_T_DEFINED) && !defined(_SIZE_T_DECLARED)
	#define _SIZE_T
	#define _SIZE_T_DECLARED
	#define _SIZE_T_DEFINED
	typedef unsigned long size_t;
#endif

#define TO_B64(X) ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[X])

/* 
 * Encodes `_n` bytes of data (pointed to by `_s`) into
 * base-64, storing the result as a null terminating string
 * in `_d`. The number of successfully written bytes is
 * returned.
 */
int webs_b64_encode(char* _s, char* _d, size_t _n);

/* 
 * Decodes the base-64 encoded (null terminating) string
 * pointed to by `_s`, writing the resulting binary data to
 * `_d`. The number of successfully written bytes is returned.
 */
int webs_b64_decode(char* _s, char* _d);

#endif /* __WEBS_BASE_64_H__ */
