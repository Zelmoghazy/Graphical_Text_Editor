#include "../include/util.h"


void log_error(int error_code)
{
    if (error_code != 0) {
        DEBUG_PRT("SDL ERROR : %s", SDL_GetError());
        exit(1);
    }
}

void* check_ptr (void *ptr)
{
    if(ptr == NULL)
    {
        DEBUG_PRT("SDL NULL ptr : %s", SDL_GetError());
        exit(1);
    }
    return ptr;
}

void buf_append(abuf *ab, const char *s, int len) 
{
    char *new_buf = (char *)realloc(ab->b, ab->len + len);
    if (new_buf == NULL) return;
    memcpy(&new_buf[ab->len], s, len);
    ab->b = new_buf;
    ab->len += len;
}

void buf_free(abuf *ab) 
{
    free(ab->b);
}

ssize_t getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp)
{
	char *ptr, *eptr;

	if (*buf == NULL || *bufsiz == 0) {
		*bufsiz = BUFSIZ;
		if ((*buf = (char *)malloc(*bufsiz)) == NULL)
			return -1;
	}

	for (ptr = *buf, eptr = *buf + *bufsiz;;) {
		int c = fgetc(fp);
		if (c == -1) {
			if (feof(fp))
				return ptr == *buf ? -1 : ptr - *buf;
			else
				return -1;
		}

        // expand tabs to spaces
        if(c == '\t')
        {
            for(size_t i = 0; i < TAB_SIZE; i++){
                *ptr++ = ' ';
                if (ptr + 2 >= eptr) {
                    char *nbuf;
                    size_t nbufsiz = *bufsiz * 2;
                    ssize_t d = ptr - *buf;
                    if ((nbuf = (char*)realloc(*buf, nbufsiz)) == NULL)
                        return -1;
                    *buf = nbuf;
                    *bufsiz = nbufsiz;
                    eptr = nbuf + nbufsiz;
                    ptr = nbuf + d;
                }
            }
            continue;
        }

		*ptr++ = c;
		if (c == delimiter) {
			*ptr = '\0';
			return ptr - *buf;
		}
		if (ptr + 2 >= eptr) {
			char *nbuf;
			size_t nbufsiz = *bufsiz * 2;
			ssize_t d = ptr - *buf;
			if ((nbuf = (char *)realloc(*buf, nbufsiz)) == NULL)
				return -1;
			*buf = nbuf;
			*bufsiz = nbufsiz;
			eptr = nbuf + nbufsiz;
			ptr = nbuf + d;
		}
	}
}

ssize_t getline(char **buf, size_t *bufsiz, FILE *fp)
{
	return getdelim(buf, bufsiz, '\n', fp);
}

vec2f_t vec2f(float x, float y)
{
    vec2f_t vec2 = {
        .x = x,
        .y = y,
    };
    return  vec2;
}

vec2f_t vec2f_add(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x + b.x;
    vec2.y = a.y + b.y;
    return vec2;
}

vec2f_t vec2f_sub(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x - b.x;
    vec2.y = a.y - b.y;
    return vec2;
}

vec2f_t vec2f_mul(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x * b.x;
    vec2.y = a.y * b.y;
    return vec2;
}

vec2f_t vec2f_div(vec2f_t a, vec2f_t b)
{
    vec2f_t vec2;
    vec2.x = a.x / b.x;
    vec2.y = a.y / b.y;
    return vec2;
}
