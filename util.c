#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
NOTE: writing these to STDOUT is a disconnect from the example, but it's a bit
annoying to have to pass that append_buffer around throughout all of this code.

Really, we've got some String-ish `Command` type, and we want to write it's
`String` representation into a `MutableBuffer`. Doing so has the desired side
effects.
*/

void clearDisplayForStandardOut(void) { write(STDOUT_FILENO, "\x1b[2J", 4); }
void repositionCursorToTopLeft(void) { write(STDOUT_FILENO, "\x1b[H", 3); }
void hideCursorWhileWriting(void) { write(STDOUT_FILENO, "\x1b[?25l", 6); }
void displayCursorAfterWriting(void) { write(STDOUT_FILENO, "\x1b[?25h", 6); }

void die(const char *message) {
  clearDisplayForStandardOut();
  repositionCursorToTopLeft();
  perror(message);
  exit(1);
}
