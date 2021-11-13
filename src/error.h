#ifndef __CORDAC_ERROR_H__
#define __CORDAC_ERROR_H__

#define __PASTE_WRAPPER(x) __PASTE_RAW(x)
#define __PASTE_RAW(x) #x
#define __PASTE(V) __PASTE_WRAPPER(V)

#include <errno.h>

#if __STDC_VERSION__ > 199409L
	#define XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mInternal Runtime Error: \x1b[0m(in "__PASTE(__FILE__)", func: \x1b[1m%s\x1b[0m [line \x1b[1m"__PASTE(__LINE__)"\x1b[0m]) : "MESG"\n", __func__); exit(ERR); }
#else
	#define XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mInternal Runtime Error: \x1b[0m(in "__PASTE(__FILE__)", line \x1b[1m"__PASTE(__LINE__)"\x1b[0m) : "MESG"\n"); exit(ERR); }
#endif

#endif