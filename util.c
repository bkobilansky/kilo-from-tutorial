#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void writeCommand(struct TerminalCommand tc) {
  write(STDOUT_FILENO, tc.string_representation, tc.length);
}

void clearDisplayForStandardOut(void) { writeCommand(clearDisplay()); }
void repositionCursorToTopLeft(void) { writeCommand(moveCursorToTopLeft()); }
void hideCursorWhileWriting(void) { writeCommand(hideCursor()); }
void displayCursorAfterWriting(void) { writeCommand(displayCursor()); }

struct TerminalCommand clearDisplay() {
  struct TerminalCommand tc = {"\x1b[2J", 4};
  return tc;
}
struct TerminalCommand moveCursorToTopLeft() {
  struct TerminalCommand tc = {"\x1b[H", 3};
  return tc;
}
struct TerminalCommand hideCursor() {
  struct TerminalCommand tc = {"\x1b[?25l", 6};
  return tc;
}
struct TerminalCommand displayCursor() {
  struct TerminalCommand tc = {"\x1b[?25h", 6};
  return tc;
}

void die(const char *message) {
  clearDisplayForStandardOut();
  repositionCursorToTopLeft();
  perror(message);
  exit(1);
}
