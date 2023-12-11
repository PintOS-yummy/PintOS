#include <string.h>
#include <debug.h>

/* Copies SIZE bytes from SRC to DST, which must not overlap.
	 Returns DST. */
/* 
 * SRC에서 DST로 SIZE 바이트를 복사합니다. SRC와 DST는 겹치지 않아야 합니다.
 * DST를 반환합니다.
 */
void *
memcpy(void *dst_, const void *src_, size_t size)
{
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT(dst != NULL || size == 0);
	ASSERT(src != NULL || size == 0);

	while (size-- > 0)
		*dst++ = *src++;

	return dst_;
}

/* Copies SIZE bytes from SRC to DST, which are allowed to
	 overlap.  Returns DST. */
void *
memmove(void *dst_, const void *src_, size_t size)
{
	unsigned char *dst = dst_;
	const unsigned char *src = src_;

	ASSERT(dst != NULL || size == 0);
	ASSERT(src != NULL || size == 0);

	if (dst < src)
	{
		while (size-- > 0)
			*dst++ = *src++;
	}
	else
	{
		dst += size;
		src += size;
		while (size-- > 0)
			*--dst = *--src;
	}

	return dst;
}

/* Find the first differing byte in the two blocks of SIZE bytes
	 at A and B.  Returns a positive value if the byte in A is
	 greater, a negative value if the byte in B is greater, or zero
	 if blocks A and B are equal. */
/*
 * A와 B에서 SIZE 바이트의 두 블록에서 첫 번째로 다른 바이트를 찾습니다.
 * A의 바이트가 더 크면 양수 값을 반환하고, B의 바이트가 더 크면 음수 값을 반환합니다.
 * 만약 A와 B 블록이 같다면 0을 반환합니다.
 */
int memcmp(const void *a_, const void *b_, size_t size)
{
	const unsigned char *a = a_;
	const unsigned char *b = b_;

	ASSERT(a != NULL || size == 0);
	ASSERT(b != NULL || size == 0);

	for (; size-- > 0; a++, b++)
		if (*a != *b)
			return *a > *b ? +1 : -1;
	return 0;
}

/* Finds the first differing characters in strings A and B.
	 Returns a positive value if the character in A (as an unsigned
	 char) is greater, a negative value if the character in B (as
	 an unsigned char) is greater, or zero if strings A and B are
	 equal. */
/*
 * 문자열 A와 B에서 처음으로 다른 문자를 찾습니다.
 * A의 문자(부호 없는 char로)가 더 크면 양의 값을 반환하고,
 * B의 문자(부호 없는 char로)가 더 크면 음의 값을 반환합니다.
 * 만약 문자열 A와 B가 같다면 0을 반환합니다.
 * 'A'는 65, 'B'는 66
 */
int strcmp(const char *a_, const char *b_)
{
	const unsigned char *a = (const unsigned char *)a_;
	const unsigned char *b = (const unsigned char *)b_;

	ASSERT(a != NULL);
	ASSERT(b != NULL);

	while (*a != '\0' && *a == *b)
	{
		a++;
		b++;
	}

	return *a < *b ? -1 : *a > *b;
}

/* Returns a pointer to the first occurrence of CH in the first
	 SIZE bytes starting at BLOCK.  Returns a null pointer if CH
	 does not occur in BLOCK. */
/*
 * BLOCK에서 시작하는 첫 SIZE 바이트 내에서 CH의 첫 번째 발생 위치를 가리키는 포인터를 반환합니다.
 * BLOCK 내에서 CH가 발견되지 않으면 널 포인터를 반환합니다.
 */
void *
memchr(const void *block_, int ch_, size_t size)
{
	const unsigned char *block = block_;
	unsigned char ch = ch_;

	ASSERT(block != NULL || size == 0);

	for (; size-- > 0; block++)
		if (*block == ch)
			return (void *)block;

	return NULL;
}

/* Finds and returns the first occurrence of C in STRING, or a
	 null pointer if C does not appear in STRING.  If C == '\0'
	 then returns a pointer to the null terminator at the end of
	 STRING. */
/*
 * STRING에서 C의 첫 번째 발생을 찾아 반환합니다. 만약 STRING 내에 C가
 * 나타나지 않는 경우, null 포인터를 반환합니다. C가 '\0'인 경우에는
 * STRING의 끝에 있는 null 종결자(null terminator)를 가리키는 포인터를 반환합니다.
 */
char *
strchr(const char *string, int c_)
{
	char c = c_;

	ASSERT(string);

	for (;;)
		if (*string == c)
			return (char *)string;
		else if (*string == '\0')
			return NULL;
		else
			string++;
}

/* Returns the length of the initial substring of STRING that
	 consists of characters that are not in STOP. */
/*
 * STRING의 초기 부분 문자열 중에서 STOP에 포함되지 않은 문자로만
 * 구성된 부분의 길이를 반환합니다.
 * 문자열에서 특정 문자 집합에 포함되지 않는 첫 부분의 크기를 측정하는 데 사용
 */
size_t
strcspn(const char *string, const char *stop)
{
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr(stop, string[length]) != NULL)
			break;
	return length;
}

/* Returns a pointer to the first character in STRING that is
	 also in STOP.  If no character in STRING is in STOP, returns a
	 null pointer. */
/*
 * STRING 내에서 STOP에도 포함된 첫 번째 문자를 가리키는 포인터를 반환합니다.
 * 만약 STRING의 어떤 문자도 STOP에 포함되어 있지 않다면, null 포인터를 반환합니다.
 */
char *
strpbrk(const char *string, const char *stop)
{
	for (; *string != '\0'; string++)
		if (strchr(stop, *string) != NULL)
			return (char *)string;
	return NULL;
}

/* Returns a pointer to the last occurrence of C in STRING.
	 Returns a null pointer if C does not occur in STRING. */
/*
 * STRING 내에서 C의 마지막 발생을 가리키는 포인터를 반환합니다.
 * 만약 STRING 내에서 C가 발생하지 않는 경우, null 포인터를 반환합니다.
 */
char *
strrchr(const char *string, int c_)
{
	char c = c_;
	const char *p = NULL;

	for (; *string != '\0'; string++)
		if (*string == c)
			p = string;
	return (char *)p;
}

/* Returns the length of the initial substring of STRING that
	 consists of characters in SKIP. */
/*
 * STRING의 초기 부분 문자열 중에서 SKIP에 포함된 문자로만
 * 구성된 부분의 길이를 반환합니다.
 */
size_t
strspn(const char *string, const char *skip)
{
	size_t length;

	for (length = 0; string[length] != '\0'; length++)
		if (strchr(skip, string[length]) == NULL)
			break;
	return length;
}

/* Returns a pointer to the first occurrence of NEEDLE within
	 HAYSTACK.  Returns a null pointer if NEEDLE does not exist
	 within HAYSTACK. */
/*
 * HAYSTACK 내에서 NEEDLE의 첫 번째 발생 위치를 가리키는 포인터를 반환합니다.
 * 만약 HAYSTACK 내에 NEEDLE이 존재하지 않는 경우, null 포인터를 반환합니다.
 */
char *
strstr(const char *haystack, const char *needle)
{
	size_t haystack_len = strlen(haystack);
	size_t needle_len = strlen(needle);

	if (haystack_len >= needle_len)
	{
		size_t i;

		for (i = 0; i <= haystack_len - needle_len; i++)
			if (!memcmp(haystack + i, needle, needle_len))
				return (char *)haystack + i;
	}

	return NULL;
}

/* Breaks a string into tokens separated by DELIMITERS.  The
	 first time this function is called, S should be the string to
	 tokenize, and in subsequent calls it must be a null pointer.
	 SAVE_PTR is the address of a `char *' variable used to keep
	 track of the tokenizer's position.  The return value each time
	 is the next token in the string, or a null pointer if no
	 tokens remain.

	 This function treats multiple adjacent delimiters as a single
	 delimiter.  The returned tokens will never be length 0.
	 DELIMITERS may change from one call to the next within a
	 single string.

	 strtok_r() modifies the string S, changing delimiters to null
	 bytes.  Thus, S must be a modifiable string.  String literals,
	 in particular, are *not* modifiable in C, even though for
	 backward compatibility they are not `const'.

	 Example usage:

	 char s[] = "  String to  tokenize. ";
	 char *token, *save_ptr;

	 for (token = strtok_r (s, " ", &save_ptr); token != NULL;
	 token = strtok_r (NULL, " ", &save_ptr))
	 printf ("'%s'\n", token);

outputs:

'String'
'to'
'tokenize.'
*/
/*
 * 문자열을 DELIMITERS로 분리된 토큰으로 나눕니다.
 * 이 함수를 처음 호출할 때 S는 토큰화할 문자열이어야 하며,
 * 이후 호출에서는 NULL 포인터가 되어야 합니다.
 * SAVE_PTR은 토크나이저의 위치를 추적하는 데 사용되는 `char *` 변수의 주소입니다.
 * 매번 반환되는 값은 문자열의 다음 토큰이거나, 더 이상 토큰이 없으면 NULL 포인터입니다.
 *
 * 이 함수는 연속된 구분자를 하나의 구분자로 취급합니다.
 * 반환된 토큰은 길이가 0이 될 수 없습니다.
 * DELIMITERS는 하나의 문자열 내에서 다음 호출로 변경될 수 있습니다.
 *
 * strtok_r()은 문자열 S를 수정하여 구분자를 널 바이트로 변경합니다.
 * 따라서, S는 수정 가능한 문자열이어야 합니다.
 * 특히 문자열 리터럴은 C에서 수정할 수 *없습니다*,
 * 비록 역호환성을 위해 `const`가 아니더라도 말입니다.
 *
 * 사용 예시:
 *
 * char s[] = "  문자열을  토큰화합니다. ";
 * char *token, *save_ptr;
 * for (token = strtok_r (s, " ", &save_ptr); token != NULL;
 * 	token = strtok_r (NULL, " ", &save_ptr))
 * 	printf ("'%s'\n", token);
 *
 * 출력 결과:
 *
 * '문자열을'
 * '토큰화합니다.'
 */
char *
strtok_r(char *s, const char *delimiters, char **save_ptr)
{
	char *token;

	ASSERT(delimiters != NULL);
	ASSERT(save_ptr != NULL);

	/* If S is nonnull, start from it.
		 If S is null, start from saved position. */
	if (s == NULL)
		s = *save_ptr;
	ASSERT(s != NULL);

	/* Skip any DELIMITERS at our current position. */
	while (strchr(delimiters, *s) != NULL)
	{
		/* strchr() will always return nonnull if we're searching
			 for a null byte, because every string contains a null
			 byte (at the end). */
		if (*s == '\0')
		{
			*save_ptr = s;
			return NULL;
		}

		s++;
	}

	/* Skip any non-DELIMITERS up to the end of the string. */
	token = s;
	while (strchr(delimiters, *s) == NULL)
		s++;
	if (*s != '\0')
	{
		*s = '\0';
		*save_ptr = s + 1;
	}
	else
		*save_ptr = s;
	return token;
}

/* Sets the SIZE bytes in DST to VALUE. */
/*
 * DST의 SIZE 바이트를 VALUE로 설정합니다.
 * 문자열이나 배열 등의 메모리 블록을 특정 값으로 초기화할 때 사용
 */
void *
memset(void *dst_, int value, size_t size)
{
	/*
	 * dst 포인터는 dst_를 unsigned char 포인터로 변환하여 처리합니다.
	 * 이렇게 함으로써 바이트 단위로 접근 할 수 있음
	 */
	unsigned char *dst = dst_;

	ASSERT(dst != NULL || size == 0);

	while (size-- > 0) // size가 0이 될 때까지 루프를 계속 실행
		*dst++ = value;

	return dst_;
}

/* Returns the length of STRING. */
/* 
 * STRING의 길이를 반환합니다.
 */
size_t
strlen(const char *string)
{
	const char *p;

	ASSERT(string);

	for (p = string; *p != '\0'; p++)
		continue;
	return p - string;
}

/* If STRING is less than MAXLEN characters in length, returns
	 its actual length.  Otherwise, returns MAXLEN. */
/* 
 * 문자열 STRING이 MAXLEN 문자보다 짧은 길이를 가지고 있다면,
 * 실제 길이를 반환합니다. 그렇지 않다면, MAXLEN을 반환합니다.
 */
size_t
strnlen(const char *string, size_t maxlen)
{
	size_t length;

	for (length = 0; string[length] != '\0' && length < maxlen; length++)
		continue;
	return length;
}

/* Copies string SRC to DST.  If SRC is longer than SIZE - 1
	 characters, only SIZE - 1 characters are copied.  A null
	 terminator is always written to DST, unless SIZE is 0.
	 Returns the length of SRC, not including the null terminator.

	 strlcpy() is not in the standard C library, but it is an
	 increasingly popular extension.  See
http://www.courtesan.com/todd/papers/strlcpy.html for
information on strlcpy(). */
/* 
 * 문자열 SRC를 DST로 복사합니다. SRC가 SIZE - 1 문자보다 길 경우,
 * 오직 SIZE - 1 문자만이 복사됩니다. null 종결자는 항상 DST에
 * 쓰여지며, SIZE가 0이 아닌 경우에만 적용됩니다.
 * 반환 값은 null 종결자를 포함하지 않은 SRC의 길이입니다.
 *
 * strlcpy()는 표준 C 라이브러리에는 포함되어 있지 않지만,
 * 점점 더 많이 사용되는 확장 기능입니다. strlcpy()에 대한
 * 정보는 http://www.courtesan.com/todd/papers/strlcpy.html 에서 확인할 수 있습니다.
 */
size_t
strlcpy(char *dst, const char *src, size_t size)
{
	size_t src_len;

	ASSERT(dst != NULL);
	ASSERT(src != NULL);

	src_len = strlen(src);
	if (size > 0)
	{
		size_t dst_len = size - 1;
		if (src_len < dst_len)
			dst_len = src_len;
		memcpy(dst, src, dst_len);
		dst[dst_len] = '\0';
	}
	return src_len;
}

/* Concatenates string SRC to DST.  The concatenated string is
	 limited to SIZE - 1 characters.  A null terminator is always
	 written to DST, unless SIZE is 0.  Returns the length that the
	 concatenated string would have assuming that there was
	 sufficient space, not including a null terminator.

	 strlcat() is not in the standard C library, but it is an
	 increasingly popular extension.  See
http://www.courtesan.com/todd/papers/strlcpy.html for
information on strlcpy(). */
/* 
 * 문자열 SRC를 DST에 연결합니다. 연결된 문자열은 SIZE - 1 문자로 제한됩니다.
 * null 종결자는 항상 DST에 쓰여지며, SIZE가 0이 아닌 경우에만 적용됩니다.
 * 반환 값은 충분한 공간이 있다고 가정했을 때 연결된 문자열의 길이이며,
 * null 종결자는 포함되지 않습니다.
 *
 * strlcat()은 표준 C 라이브러리에는 포함되어 있지 않지만,
 * 점점 더 많이 사용되는 확장 기능입니다. strlcpy()에 대한
 * 정보는 http://www.courtesan.com/todd/papers/strlcpy.html 에서 확인할 수 있습니다.
 */
size_t
strlcat(char *dst, const char *src, size_t size)
{
	size_t src_len, dst_len;

	ASSERT(dst != NULL);
	ASSERT(src != NULL);

	src_len = strlen(src);
	dst_len = strlen(dst);
	if (size > 0 && dst_len < size)
	{
		size_t copy_cnt = size - dst_len - 1;
		if (src_len < copy_cnt)
			copy_cnt = src_len;
		memcpy(dst + dst_len, src, copy_cnt);
		dst[dst_len + copy_cnt] = '\0';
	}
	return src_len + dst_len;
}
