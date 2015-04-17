
#include <kernel.h>


int k_strlen(const char* str)
{
	const char* end = str;
	while(*end != '\0') ++end;
	return end - str;
}

void* k_memcpy(void* dst, const void* src, int len)
{
	char* d = (char*)dst;
	const char* s = (char*)src;
	const char* end = d + len;
	while(d < end) *d++ = *s++;
	return dst;
}

int k_memcmp(const void* b1, const void* b2, int len)
{
	int i;
	const char* a = (char*)b1;
	const char* b = (char*)b2;
	const char* end = a + len;
	while(a < end) 
	{
		i = *a++ - *b++;
		if (i != 0) return i;
	}
	return 0;
}

int k_strcmp(const char* str1, const char* str2)
{
	int i;
	while(*str1 != '\0' && *str2 != '\0') 
	{
		i = *str1++ - *str2++;
		if (i != 0) return i;
	}
	return *str1 - *str2;
}

void *k_memset(void *dst, int c, int len)
{
	char* mem = (char*)dst;
	const char* end = mem + len;
	while(mem < end) *mem++ = (char)c;
	return dst;
}

int atoi(const char* str)
{
	int num = 0;
	BOOL started = FALSE;
	while (*str != '\0')
	{
		if (*str >= '0' && *str <= '9')
		{
			num = num * 10 + *str - '0';
			started = TRUE;
		}
		else if (!started && (*str == ' ' || *str == '\t'))
		{
			continue;
		}
		else
		{
			break;
		}
		str++;
	}
	return num;
}
