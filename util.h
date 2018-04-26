#ifndef util_h
#define util_h

struct TerminalCommand {
  char *string_representation;
  int length;
};

struct TerminalCommand clearDisplay(void);
struct TerminalCommand moveCursorToTopLeft(void);
struct TerminalCommand hideCursor(void);
struct TerminalCommand displayCursor(void);

#pragma mark - Execute commands immediately via STDOUT

void clearDisplayForStandardOut(void);
void repositionCursorToTopLeft(void);
void hideCursorWhileWriting(void);
void displayCursorAfterWriting(void);

#pragma mark - Error Handling

// print an error message and exit with a non-zero status.
void die(const char *message);

#endif
