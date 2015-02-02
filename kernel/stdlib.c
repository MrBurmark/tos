
#include <kernel.h>


int k_strlen(const char* str)
{
	char* end = (char*)str;
	while(*end != '\0') ++end;
	return end - str;
}

void* k_memcpy(void* dst, const void* src, int len)
{
	char* d = (char*)dst;
	char* s = (char*)src;
	char* end = d + len;
	while(d < end) *d++ = *s++;
	return dst;
}

int k_memcmp(const void* b1, const void* b2, int len)
{
	char* a = (char*)b1;
	char* b = (char*)b2;
	char* end = a + len;
	while(a < end && *a == *b) ++a, ++b;
	return (len == 0) ? 0 : (int)*a - (int)*b;
}

void *k_memset(void *str, int c, int len)
{
	char* mem = (char*)str;
	char* end = mem + len;
	while(mem < end) *mem++ = (char)c;
	return str;
}
