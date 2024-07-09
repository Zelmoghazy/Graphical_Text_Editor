#ifndef UTIL_H_
#define UTIL_H_

#include <SDL2/SDL_error.h>

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TAB_SIZE 4

#define DEBUG  1

#define DEBUG_PRT(fmt, ...)                                                                                  \
    do{                                                                                                      \
        if(DEBUG)                                                                                            \
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__);     \
    }while(0)

#define max(a, b) ((a) > (b) ? (a) : (b))

#define ABUF_INIT() {NULL, 0}


typedef struct vec2f_t{
    float x;
    float y;
}vec2f_t;

typedef struct abuf {
  char *b;
  int len;
}abuf;

void log_error(int error_code);
void* check_ptr (void *ptr);

void buf_append(abuf *ab, const char *s, int len);
void buf_free(abuf *ab);

ssize_t getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp);
ssize_t getline(char **buf, size_t *bufsiz, FILE *fp);

vec2f_t vec2f(float x, float y);
vec2f_t vec2f_add(vec2f_t a, vec2f_t b);
vec2f_t vec2f_sub(vec2f_t a, vec2f_t b);
vec2f_t vec2f_mul(vec2f_t a, vec2f_t b);
vec2f_t vec2f_div(vec2f_t a, vec2f_t b);

#endif /* UTIL_H_ */