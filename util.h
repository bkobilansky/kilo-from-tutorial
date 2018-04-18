#ifndef util_h
#define util_h

void clearDisplayForStandardOut(void);
void repositionCursorToTopLeft(void);
void hideCursorWhileWriting(void);
void displayCursorAfterWriting(void);

// print an error message and exit with a non-zero status.
void die(const char *message);

#endif
