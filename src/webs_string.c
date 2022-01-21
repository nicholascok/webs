#include "webs_string.h"

int str_cat(char* _buf, char* _a, char* _b) {
	int len = 0;
	while (*_a) *(_buf++) = *(_a++), len++;
	while (*_b) *(_buf++) = *(_b++), len++;
	*_buf = '\0';
	return len;
}

int str_cmp(char* _a, char* _b) {
	int i;
	for (i = 0; _a[i] == _b[i]; i++)
		if (!_a[i] && !_b[i]) return 1;
	return 0;
}

int str_cmp_insensitive(char* _a, char* _b) {
	int i;
	for (i = 0; _a[i] == _b[i] || _a[i] == _b[i] - 0x20 || _a[i] == _b[i] + 0x20; i++)
		if (!_a[i] && !_b[i]) return 1;
	return 0;
}
