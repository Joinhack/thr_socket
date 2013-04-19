#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>
#include "common.h"

void time_now(long *s, int *ms) {
	struct timeval now;
	gettimeofday(&now, NULL);
	*s = now.tv_sec;
	*ms = now.tv_usec/1000;
}

int str2ll(char *p, size_t len, long long *l) {
	unsigned long long v = 0;
	char *ptr = p, c;
	int negtive = 0, i;
	if(ptr[0] == '-') {
		ptr++;
		negtive = 1;
	}
	while(ptr != p + len) {
		c = ptr[0];
		if(c < '0' || c > '9')
			return -1;
		v *= 10;
		i = ptr[0] - '0';
		if(v > ULLONG_MAX - i)
			return -1;
		v += i;
		ptr++;
	}
	if(negtive) {
		if(v > (unsigned long long)LLONG_MIN)
			return -1;
		*l = -v;
	} else {
		if(v > LLONG_MAX)
			return -1;
		*l = v;
	}
	return 0;
}

int str2l(char *p, size_t len, long *l) {
	int rs;
	long long ll;
	rs = str2ll(p, len, &ll);
	if(rs < 0) return rs;
	if(ll > (long long)LONG_MAX) return -1;
	if(ll < (long long)LONG_MIN) return -1;
	*l = ll;
	return 0;
}

int ll2str(long long l, char *p, size_t size) {
	char *ptr, buf[64];
	int len = 0, bs, offset;
	unsigned long long ll;
	bs = sizeof(buf);
	memset(buf, 0, bs);
	ptr = buf + bs;
	ll = l < 0?-l:l;
	do {
		*(--ptr) = '0' + ll%10;
		ll /= 10;
	} while(ll);
	offset = (buf + bs) - ptr;
	len =  offset;
	if(l < 0) {
		if(offset > size - 1) return -1;	
		p[0] = '-';
		p++;
		len++;
	} else {
		if(offset > size) return -1;	
	}
	memcpy(p, ptr, offset);
	return len;
}

