#ifndef __X_ERROR_H__
#define __X_ERROR_H__

#define __XE_PASTE_WRAPPER(x) __XE_PASTE_RAW(x)
#define __XE_PASTE_RAW(x) #x
#define __XE_PASTE(V) __XE_PASTE_WRAPPER(V)

#include <errno.h>

#define XEMSG_NOMEM "failed to allocate memory!"

#if __STDC_VERSION__ > 199409L
	#ifdef NOESCAPE
		#define XERR(MESG, ERR) { printf("Runtime Error: (in "__XE_PASTE(__FILE__)", func: %s [line "__XE_PASTE(__LINE__)"]) : "MESG"\n", __func__); exit(ERR); }
	#else
		#define XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mRuntime Error: \x1b[0m(in "__XE_PASTE(__FILE__)", func: \x1b[1m%s\x1b[0m [line \x1b[1m"__XE_PASTE(__LINE__)"\x1b[0m]) : "MESG"\n", __func__); exit(ERR); }
	#endif
#else
	#ifdef NOESCAPE
		#define XERR(MESG, ERR) { printf("Runtime Error: (in "__XE_PASTE(__FILE__)", line "__XE_PASTE(__LINE__)") : "MESG"\n"); exit(ERR); }
	#else
		#define XERR(MESG, ERR) { printf("\x1b[31m\x1b[1mRuntime Error: \x1b[0m(in "__XE_PASTE(__FILE__)", line \x1b[1m"__XE_PASTE(__LINE__)"\x1b[0m) : "MESG"\n"); exit(ERR); }
	#endif
#endif

#endif /* __X_ERROR_H__ */
