#include "append-buffer.h"
#include <stdlib.h>
#include <string.h>

#define append_buffer_init {NULL, 0};

void append_buffer_append(struct append_buffer *ab, char *string, int length) {
  char *longerBuffer = realloc(ab->b, ab->length + length);

  if (longerBuffer == NULL) {
    return;
  }

  memcpy(&longerBuffer[ab->length], string, length);
  ab->b = longerBuffer;
  ab->length += length;
}

void append_buffer_free(struct append_buffer *ab) { free(ab->b); }
