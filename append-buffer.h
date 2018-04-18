#ifndef append_buffer_h
#define append_buffer_h

#include <stdlib.h>
#include <string.h>

// a simple dynamic length string
struct append_buffer {
  char *b;
  int length;
};

#define append_buffer_init {NULL, 0};

void append_buffer_append(struct append_buffer *ab, char *string, int length);

void append_buffer_free(struct append_buffer *ab);

#endif
